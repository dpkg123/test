/* General functions for the WDB TUI.

   Copyright 1998, 1999, 2000, 2001, 2002 Free Software Foundation,
   Inc.

   Contributed by Hewlett-Packard Company.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* FIXME: cagney/2002-02-28: The GDB coding standard indicates that
   "defs.h" should be included first.  Unfortunatly some systems
   (currently Debian GNU/Linux) include the <stdbool.h> via <curses.h>
   and they clash with "bfd.h"'s definiton of true/false.  The correct
   fix is to remove true/false from "bfd.h", however, until that
   happens, hack around it by including "config.h" and <curses.h>
   first.  */

#include "config.h"
#ifdef HAVE_NCURSES_H       
#include <ncurses.h>
#else
#ifdef HAVE_CURSES_H
#include <curses.h>
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <malloc.h>
#ifdef HAVE_TERM_H
#include <term.h>
#endif
#include <signal.h>
#include <fcntl.h>
#if 0
#include <termio.h>
#endif
#include <setjmp.h>
#include "defs.h"
#include "gdbcmd.h"
#include "tui.h"
#include "tuiData.h"
#include "tuiLayout.h"
#include "tuiIO.h"
#include "tuiRegs.h"
#include "tuiStack.h"
#include "tuiWin.h"
#include "tuiSourceWin.h"
#include "readline/readline.h"
#include "target.h"
#include "frame.h"
#include "breakpoint.h"
#include "inferior.h"
#include "symtab.h"
#include "source.h"

/* Tells whether the TUI is active or not.  */
int tui_active = 0;
static int tui_finish_init = 1;

enum tui_key_mode tui_current_key_mode = tui_command_mode;

struct tui_char_command
{
  unsigned char key;
  const char* cmd;
};

/* Key mapping to gdb commands when the TUI is using the single key mode.  */
static const struct tui_char_command tui_commands[] = {
  { 'c', "continue" },
  { 'd', "down" },
  { 'f', "finish" },
  { 'n', "next" },
  { 'r', "run" },
  { 's', "step" },
  { 'u', "up" },
  { 'v', "info locals" },
  { 'w', "where" },
  { 0, 0 },
};

static Keymap tui_keymap;
static Keymap tui_readline_standard_keymap;

/* TUI readline command.
   Switch the output mode between TUI/standard gdb.  */
static int
tui_rl_switch_mode (void)
{
  if (tui_active)
    {
      tui_disable ();
      rl_prep_terminal (0);
    }
  else
    {
      rl_deprep_terminal ();
      tui_enable ();
    }

  /* Clear the readline in case switching occurred in middle of something.  */
  if (rl_end)
    rl_kill_text (0, rl_end);

  /* Since we left the curses mode, the terminal mode is restored to
     some previous state.  That state may not be suitable for readline
     to work correctly (it may be restored in line mode).  We force an
     exit of the current readline so that readline is re-entered and it
     will be able to setup the terminal for its needs.  By re-entering
     in readline, we also redisplay its prompt in the non-curses mode.  */
  rl_newline (1, '\n');

  /* Make sure the \n we are returning does not repeat the last command.  */
  dont_repeat ();
  return 0;
}

/* TUI readline command.
   Change the TUI layout to show a next layout.
   This function is bound to CTRL-X 2.  It is intended to provide
   a functionality close to the Emacs split-window command.  We always
   show two windows (src+asm), (src+regs) or (asm+regs).  */
static int
tui_rl_change_windows (void)
{
  if (!tui_active)
    tui_rl_switch_mode ();

  if (tui_active)
    {
      TuiLayoutType new_layout;
      TuiRegisterDisplayType regs_type = TUI_UNDEFINED_REGS;

      new_layout = currentLayout ();

      /* Select a new layout to have a rolling layout behavior
	 with always two windows (except when undefined).  */
      switch (new_layout)
	{
	case SRC_COMMAND:
	  new_layout = SRC_DISASSEM_COMMAND;
	  break;

	case DISASSEM_COMMAND:
	  new_layout = SRC_DISASSEM_COMMAND;
	  break;

	case SRC_DATA_COMMAND:
	  new_layout = SRC_DISASSEM_COMMAND;
	  break;

	case SRC_DISASSEM_COMMAND:
	  new_layout = DISASSEM_DATA_COMMAND;
	  break;
	  
	case DISASSEM_DATA_COMMAND:
	  new_layout = SRC_DATA_COMMAND;
	  break;

	default:
	  new_layout = SRC_COMMAND;
	  break;
	}
      tuiSetLayout (new_layout, regs_type);
    }
  return 0;
}

/* TUI readline command.
   Delete the second TUI window to only show one.  */
static int
tui_rl_delete_other_windows (void)
{
  if (!tui_active)
    tui_rl_switch_mode ();

  if (tui_active)
    {
      TuiLayoutType new_layout;
      TuiRegisterDisplayType regs_type = TUI_UNDEFINED_REGS;

      new_layout = currentLayout ();

      /* Kill one window.  */
      switch (new_layout)
	{
	case SRC_COMMAND:
	case SRC_DATA_COMMAND:
	case SRC_DISASSEM_COMMAND:
	default:
	  new_layout = SRC_COMMAND;
	  break;

	case DISASSEM_COMMAND:
	case DISASSEM_DATA_COMMAND:
	  new_layout = DISASSEM_COMMAND;
	  break;
	}
      tuiSetLayout (new_layout, regs_type);
    }
  return 0;
}

/* TUI readline command.
   Execute the gdb command bound to the specified key.  */
static int
tui_rl_command_key (int count, int key)
{
  int i;

  reinitialize_more_filter ();
  for (i = 0; tui_commands[i].cmd; i++)
    {
      if (tui_commands[i].key == key)
        {
          /* Must save the command because it can be modified
             by execute_command.  */
          char* cmd = alloca (strlen (tui_commands[i].cmd) + 1);
          strcpy (cmd, tui_commands[i].cmd);
          execute_command (cmd, TRUE);
          return 0;
        }
    }
  return 0;
}

/* TUI readline command.
   Temporarily leave the TUI SingleKey mode to allow editing
   a gdb command with the normal readline.  Once the command
   is executed, the TUI SingleKey mode is installed back.  */
static int
tui_rl_command_mode (int count, int key)
{
  tui_set_key_mode (tui_one_command_mode);
  return rl_insert (count, key);
}

/* TUI readline command.
   Switch between TUI SingleKey mode and gdb readline editing.  */
static int
tui_rl_next_keymap (void)
{
  tui_set_key_mode (tui_current_key_mode == tui_command_mode
                    ? tui_single_key_mode : tui_command_mode);
  return 0;
}

/* Readline hook to redisplay ourself the gdb prompt.
   In the SingleKey mode, the prompt is not printed so that
   the command window is cleaner.  It will be displayed if
   we temporarily leave the SingleKey mode.  */
static int
tui_rl_startup_hook ()
{
  rl_already_prompted = 1;
  if (tui_current_key_mode != tui_command_mode)
    tui_set_key_mode (tui_single_key_mode);
  tui_redisplay_readline ();
  return 0;
}

/* Change the TUI key mode by installing the appropriate readline keymap.  */
void
tui_set_key_mode (enum tui_key_mode mode)
{
  tui_current_key_mode = mode;
  rl_set_keymap (mode == tui_single_key_mode
                 ? tui_keymap : tui_readline_standard_keymap);
  tuiShowLocatorContent ();
}

/* Initialize readline and configure the keymap for the switching
   key shortcut.  */
void
tui_initialize_readline ()
{
  int i;
  Keymap tui_ctlx_keymap;

  rl_initialize ();

  rl_add_defun ("tui-switch-mode", tui_rl_switch_mode, -1);
  rl_add_defun ("gdb-command", tui_rl_command_key, -1);
  rl_add_defun ("next-keymap", tui_rl_next_keymap, -1);

  tui_keymap = rl_make_bare_keymap ();
  tui_ctlx_keymap = rl_make_bare_keymap ();
  tui_readline_standard_keymap = rl_get_keymap ();

  for (i = 0; tui_commands[i].cmd; i++)
    rl_bind_key_in_map (tui_commands[i].key, tui_rl_command_key, tui_keymap);

  rl_generic_bind (ISKMAP, "\\C-x", (char*) tui_ctlx_keymap, tui_keymap);

  /* Bind all other keys to tui_rl_command_mode so that we switch
     temporarily from SingleKey mode and can enter a gdb command.  */
  for (i = ' '; i < 0x7f; i++)
    {
      int j;

      for (j = 0; tui_commands[j].cmd; j++)
        if (tui_commands[j].key == i)
          break;

      if (tui_commands[j].cmd)
        continue;

      rl_bind_key_in_map (i, tui_rl_command_mode, tui_keymap);
    }

  rl_bind_key_in_map ('a', tui_rl_switch_mode, emacs_ctlx_keymap);
  rl_bind_key_in_map ('a', tui_rl_switch_mode, tui_ctlx_keymap);
  rl_bind_key_in_map ('A', tui_rl_switch_mode, emacs_ctlx_keymap);
  rl_bind_key_in_map ('A', tui_rl_switch_mode, tui_ctlx_keymap);
  rl_bind_key_in_map (CTRL ('A'), tui_rl_switch_mode, emacs_ctlx_keymap);
  rl_bind_key_in_map (CTRL ('A'), tui_rl_switch_mode, tui_ctlx_keymap);
  rl_bind_key_in_map ('1', tui_rl_delete_other_windows, emacs_ctlx_keymap);
  rl_bind_key_in_map ('1', tui_rl_delete_other_windows, tui_ctlx_keymap);
  rl_bind_key_in_map ('2', tui_rl_change_windows, emacs_ctlx_keymap);
  rl_bind_key_in_map ('2', tui_rl_change_windows, tui_ctlx_keymap);
  rl_bind_key_in_map ('q', tui_rl_next_keymap, tui_keymap);
  rl_bind_key_in_map ('s', tui_rl_next_keymap, emacs_ctlx_keymap);
  rl_bind_key_in_map ('s', tui_rl_next_keymap, tui_ctlx_keymap);
}

/* Enter in the tui mode (curses).
   When in normal mode, it installs the tui hooks in gdb, redirects
   the gdb output, configures the readline to work in tui mode.
   When in curses mode, it does nothing.  */
void
tui_enable (void)
{
  if (tui_active)
    return;

  /* To avoid to initialize curses when gdb starts, there is a defered
     curses initialization.  This initialization is made only once
     and the first time the curses mode is entered.  */
  if (tui_finish_init)
    {
      WINDOW *w;

      w = initscr ();
  
      cbreak ();
      noecho ();
      /*timeout (1);*/
      nodelay(w, FALSE);
      nl();
      keypad (w, TRUE);
      rl_initialize ();
      setTermHeightTo (LINES);
      setTermWidthTo (COLS);
      def_prog_mode ();

      tuiShowFrameInfo (0);
      tuiSetLayout (SRC_COMMAND, TUI_UNDEFINED_REGS);
      tuiSetWinFocusTo (srcWin);
      keypad (cmdWin->generic.handle, TRUE);
      wrefresh (cmdWin->generic.handle);
      tui_finish_init = 0;
    }
  else
    {
     /* Save the current gdb setting of the terminal.
        Curses will restore this state when endwin() is called.  */
     def_shell_mode ();
     clearok (stdscr, TRUE);
   }

  /* Install the TUI specific hooks.  */
  tui_install_hooks ();
  rl_startup_hook = tui_rl_startup_hook;

  tui_update_variables ();
  
  tui_setup_io (1);

  tui_version = 1;
  tui_active = 1;
  if (selected_frame)
     tuiShowFrameInfo (selected_frame);

  /* Restore TUI keymap.  */
  tui_set_key_mode (tui_current_key_mode);
  refresh ();

  /* Update gdb's knowledge of its terminal.  */
  target_terminal_save_ours ();
  tui_update_gdb_sizes ();
}

/* Leave the tui mode.
   Remove the tui hooks and configure the gdb output and readline
   back to their original state.  The curses mode is left so that
   the terminal setting is restored to the point when we entered.  */
void
tui_disable (void)
{
  if (!tui_active)
    return;

  /* Restore initial readline keymap.  */
  rl_set_keymap (tui_readline_standard_keymap);

  /* Remove TUI hooks.  */
  tui_remove_hooks ();
  rl_startup_hook = 0;
  rl_already_prompted = 0;

  /* Leave curses and restore previous gdb terminal setting.  */
  endwin ();

  /* gdb terminal has changed, update gdb internal copy of it
     so that terminal management with the inferior works.  */
  tui_setup_io (0);

  /* Update gdb's knowledge of its terminal.  */
  target_terminal_save_ours ();

  tui_version = 0;
  tui_active = 0;
  tui_update_gdb_sizes ();
}

/* Wrapper on top of free() to ensure that input address
   is greater than 0x0.  */
void
tuiFree (char *ptr)
{
  if (ptr != (char *) NULL)
    {
      xfree (ptr);
    }
}

void
strcat_to_buf (char *buf, int buflen, const char *itemToAdd)
{
  if (itemToAdd != (char *) NULL && buf != (char *) NULL)
    {
      if ((strlen (buf) + strlen (itemToAdd)) <= buflen)
	strcat (buf, itemToAdd);
      else
	strncat (buf, itemToAdd, (buflen - strlen (buf)));
    }
}

#if 0
/* Solaris <sys/termios.h> defines CTRL. */
#ifndef CTRL
#define CTRL(x)         (x & ~0140)
#endif

#define FILEDES         2
#define CHK(val, dft)   (val<=0 ? dft : val)

static void
_tuiReset (void)
{
  struct termio mode;

  /*
     ** reset the teletype mode bits to a sensible state.
     ** Copied tset.c
   */
#if ! defined (USG) && defined (TIOCGETC)
  struct tchars tbuf;
#endif /* !USG && TIOCGETC */
#ifdef UCB_NTTY
  struct ltchars ltc;

  if (ldisc == NTTYDISC)
    {
      ioctl (FILEDES, TIOCGLTC, &ltc);
      ltc.t_suspc = CHK (ltc.t_suspc, CTRL ('Z'));
      ltc.t_dsuspc = CHK (ltc.t_dsuspc, CTRL ('Y'));
      ltc.t_rprntc = CHK (ltc.t_rprntc, CTRL ('R'));
      ltc.t_flushc = CHK (ltc.t_flushc, CTRL ('O'));
      ltc.t_werasc = CHK (ltc.t_werasc, CTRL ('W'));
      ltc.t_lnextc = CHK (ltc.t_lnextc, CTRL ('V'));
      ioctl (FILEDES, TIOCSLTC, &ltc);
    }
#endif /* UCB_NTTY */
#ifndef USG
#ifdef TIOCGETC
  ioctl (FILEDES, TIOCGETC, &tbuf);
  tbuf.t_intrc = CHK (tbuf.t_intrc, CTRL ('?'));
  tbuf.t_quitc = CHK (tbuf.t_quitc, CTRL ('\\'));
  tbuf.t_startc = CHK (tbuf.t_startc, CTRL ('Q'));
  tbuf.t_stopc = CHK (tbuf.t_stopc, CTRL ('S'));
  tbuf.t_eofc = CHK (tbuf.t_eofc, CTRL ('D'));
  /* brkc is left alone */
  ioctl (FILEDES, TIOCSETC, &tbuf);
#endif /* TIOCGETC */
  mode.sg_flags &= ~(RAW
#ifdef CBREAK
		     | CBREAK
#endif /* CBREAK */
		     | VTDELAY | ALLDELAY);
  mode.sg_flags |= XTABS | ECHO | CRMOD | ANYP;
#else /*USG */
  ioctl (FILEDES, TCGETA, &mode);
  mode.c_cc[VINTR] = CHK (mode.c_cc[VINTR], CTRL ('?'));
  mode.c_cc[VQUIT] = CHK (mode.c_cc[VQUIT], CTRL ('\\'));
  mode.c_cc[VEOF] = CHK (mode.c_cc[VEOF], CTRL ('D'));

  mode.c_iflag &= ~(IGNBRK | PARMRK | INPCK | INLCR | IGNCR | IUCLC | IXOFF);
  mode.c_iflag |= (BRKINT | ISTRIP | ICRNL | IXON);
  mode.c_oflag &= ~(OLCUC | OCRNL | ONOCR | ONLRET | OFILL | OFDEL |
		    NLDLY | CRDLY | TABDLY | BSDLY | VTDLY | FFDLY);
  mode.c_oflag |= (OPOST | ONLCR);
  mode.c_cflag &= ~(CSIZE | PARODD | CLOCAL);
#ifndef hp9000s800
  mode.c_cflag |= (CS8 | CREAD);
#else /*hp9000s800 */
  mode.c_cflag |= (CS8 | CSTOPB | CREAD);
#endif /* hp9000s800 */
  mode.c_lflag &= ~(XCASE | ECHONL | NOFLSH);
  mode.c_lflag |= (ISIG | ICANON | ECHO | ECHOK);
  ioctl (FILEDES, TCSETAW, &mode);
#endif /* USG */

  return;
}				/* _tuiReset */
#endif

void
tui_show_source (const char *file, int line)
{
  struct symtab_and_line cursal = get_current_source_symtab_and_line ();
  /* make sure that the source window is displayed */
  tuiAddWinToLayout (SRC_WIN);

  tuiUpdateSourceWindowsWithLine (cursal.symtab, line);
  tuiUpdateLocatorFilename (file);
}

void
tui_show_assembly (CORE_ADDR addr)
{
  tuiAddWinToLayout (DISASSEM_WIN);
  tuiUpdateSourceWindowsWithAddr (addr);
}

int
tui_is_window_visible (TuiWinType type)
{
  if (tui_version == 0)
    return 0;

  if (winList[type] == 0)
    return 0;
  
  return winList[type]->generic.isVisible;
}

int
tui_get_command_dimension (int *width, int *height)
{
  if (!tui_version || !m_winPtrNotNull (cmdWin))
    {
      return 0;
    }
  
  *width = cmdWin->generic.width;
  *height = cmdWin->generic.height;
  return 1;
}
