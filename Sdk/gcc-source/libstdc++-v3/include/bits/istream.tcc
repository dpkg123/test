// istream classes -*- C++ -*-

// Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003
// Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

//
// ISO C++ 14882: 27.6.2  Output streams
//

#pragma GCC system_header

#include <locale>
#include <ostream> // For flush()

namespace std 
{
  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>::sentry::
    sentry(basic_istream<_CharT, _Traits>& __in, bool __noskipws)
    {
      if (__in.good()) 
	{
	  if (__in.tie())
	    __in.tie()->flush();
	  if (!__noskipws && (__in.flags() & ios_base::skipws))
	    {	  
	      const __int_type __eof = traits_type::eof();
	      __streambuf_type* __sb = __in.rdbuf();
	      __int_type __c = __sb->sgetc();

	      if (__in._M_check_facet(__in._M_fctype))
		while (!traits_type::eq_int_type(__c, __eof)
		       && __in._M_fctype->is(ctype_base::space, 
					     traits_type::to_char_type(__c)))
		  __c = __sb->snextc();

#ifdef _GLIBCPP_RESOLVE_LIB_DEFECTS
//195.  Should basic_istream::sentry's constructor ever set eofbit? 
	      if (traits_type::eq_int_type(__c, __eof))
		__in.setstate(ios_base::eofbit);
#endif
	    }
	}

      if (__in.good())
	_M_ok = true;
      else
	{
	  _M_ok = false;
	  __in.setstate(ios_base::failbit);
	}
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(__istream_type& (*__pf)(__istream_type&))
    {
      __pf(*this);
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(__ios_type& (*__pf)(__ios_type&))
    {
      __pf(*this);
      return *this;
    }
  
  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(ios_base& (*__pf)(ios_base&))
    {
      __pf(*this);
      return *this;
    }
  
  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(bool& __n)
    {
      sentry __cerb(*this, false);
      if (__cerb) 
	{
	  try 
	    {
	      ios_base::iostate __err = ios_base::iostate(ios_base::goodbit);
	      if (_M_check_facet(_M_fnumget))
		_M_fnumget->get(*this, 0, *this, __err, __n);
	      this->setstate(__err);
	    }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(short& __n)
    {
      sentry __cerb(*this, false);
      if (__cerb) 
	{
	  try 
	    {
	      ios_base::iostate __err = ios_base::iostate(ios_base::goodbit);
	      long __l;
	      if (_M_check_facet(_M_fnumget))
		_M_fnumget->get(*this, 0, *this, __err, __l);
#ifdef _GLIBCPP_RESOLVE_LIB_DEFECTS
	      // 118. basic_istream uses nonexistent num_get member functions.
	      if (!(__err & ios_base::failbit)
		  && (numeric_limits<short>::min() <= __l 
		      && __l <= numeric_limits<short>::max()))
		__n = __l;
	      else
                __err |= ios_base::failbit;
#endif
	      this->setstate(__err);
	    }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(unsigned short& __n)
    {
      sentry __cerb(*this, false);
      if (__cerb) 
	{
	  try 
	    {
	      ios_base::iostate __err = ios_base::iostate(ios_base::goodbit);
	      if (_M_check_facet(_M_fnumget))
		_M_fnumget->get(*this, 0, *this, __err, __n);
	      this->setstate(__err);
	    }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(int& __n)
    {
      sentry __cerb(*this, false);
      if (__cerb) 
	{
	  try 
	    {
	      ios_base::iostate __err = ios_base::iostate(ios_base::goodbit);
	      long __l;
	      if (_M_check_facet(_M_fnumget))
		_M_fnumget->get(*this, 0, *this, __err, __l);
#ifdef _GLIBCPP_RESOLVE_LIB_DEFECTS
	      // 118. basic_istream uses nonexistent num_get member functions.
	      if (!(__err & ios_base::failbit)
		  && (numeric_limits<int>::min() <= __l 
		      && __l <= numeric_limits<int>::max()))
		__n = __l;
	      else
                __err |= ios_base::failbit;
#endif
	      this->setstate(__err);
	    }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(unsigned int& __n)
    {
      sentry __cerb(*this, false);
      if (__cerb) 
	{
	  try 
	    {
	      ios_base::iostate __err = ios_base::iostate(ios_base::goodbit);
	      if (_M_check_facet(_M_fnumget))
		_M_fnumget->get(*this, 0, *this, __err, __n);
	      this->setstate(__err);
	    }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(long& __n)
    {
      sentry __cerb(*this, false);
      if (__cerb) 
	{
	  try 
	    {
	      ios_base::iostate __err = ios_base::iostate(ios_base::goodbit);
	      if (_M_check_facet(_M_fnumget))
		_M_fnumget->get(*this, 0, *this, __err, __n);
	      this->setstate(__err);
	    }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(unsigned long& __n)
    {
      sentry __cerb(*this, false);
      if (__cerb) 
	{
	  try 
	    {
	      ios_base::iostate __err = ios_base::iostate(ios_base::goodbit);
	      if (_M_check_facet(_M_fnumget))
		_M_fnumget->get(*this, 0, *this, __err, __n);
	      this->setstate(__err);
	    }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return *this;
    }

#ifdef _GLIBCPP_USE_LONG_LONG
  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(long long& __n)
    {
      sentry __cerb(*this, false);
      if (__cerb) 
	{
	  try 
	    {
	      ios_base::iostate __err = ios_base::iostate(ios_base::goodbit);
	      if (_M_check_facet(_M_fnumget))
		_M_fnumget->get(*this, 0, *this, __err, __n);
	      this->setstate(__err);
	    }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
	      __throw_exception_again;
	    }
	}
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(unsigned long long& __n)
    {
      sentry __cerb(*this, false);
      if (__cerb) 
	{
	  try 
	    {
	      ios_base::iostate __err = ios_base::iostate(ios_base::goodbit);
	      if (_M_check_facet(_M_fnumget))
		_M_fnumget->get(*this, 0, *this, __err, __n);
	      this->setstate(__err);
	    }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return *this;
    }
#endif

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(float& __n)
    {
      sentry __cerb(*this, false);
      if (__cerb) 
	{
	  try 
	    {
	      ios_base::iostate __err = ios_base::iostate(ios_base::goodbit);
	      if (_M_check_facet(_M_fnumget))
		_M_fnumget->get(*this, 0, *this, __err, __n);
	      this->setstate(__err);
	    }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(double& __n)
    {
      sentry __cerb(*this, false);
      if (__cerb) 
	{
	  try 
	    {
	      ios_base::iostate __err = ios_base::iostate(ios_base::goodbit);
	      if (_M_check_facet(_M_fnumget))
		_M_fnumget->get(*this, 0, *this, __err, __n);
	      this->setstate(__err);
	    }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(long double& __n)
    {
      sentry __cerb(*this, false);
      if (__cerb) 
	{
	  try 
	    {
	      ios_base::iostate __err = ios_base::iostate(ios_base::goodbit);
	      if (_M_check_facet(_M_fnumget))
		_M_fnumget->get(*this, 0, *this, __err, __n);
	      this->setstate(__err);
	    }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(void*& __n)
    {
      sentry __cerb(*this, false);
      if (__cerb) 
	{
	  try 
	    {
	      ios_base::iostate __err = ios_base::iostate(ios_base::goodbit);
	      if (_M_check_facet(_M_fnumget))
		_M_fnumget->get(*this, 0, *this, __err, __n);
	      this->setstate(__err);
	    }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>& 
    basic_istream<_CharT, _Traits>::
    operator>>(__streambuf_type* __sbout)
    {
       sentry __cerb(*this, false);
       if (__cerb)
	 {
	   try
	     {
	       streamsize __xtrct = 0;
	       if (__sbout)
		 {
		   __streambuf_type* __sbin = this->rdbuf();
		   __xtrct = __copy_streambufs(*this, __sbin, __sbout);
		 }
	       if (!__sbout || !__xtrct)
		 this->setstate(ios_base::failbit);
	     }
	   catch(...)
	     {
	       // 27.6.2.5.1 Common requirements.
	       // Turn this on without causing an ios::failure to be thrown.
	       this->_M_setstate(ios_base::badbit);
	       if ((this->exceptions() & ios_base::badbit) != 0)
		 __throw_exception_again;
	     }
	 }
       return *this;
    }

  template<typename _CharT, typename _Traits>
    typename basic_istream<_CharT, _Traits>::int_type
    basic_istream<_CharT, _Traits>::
    get(void)
    {
      const int_type __eof = traits_type::eof();
      int_type __c = __eof;
      _M_gcount = 0;
      sentry __cerb(*this, true);
      if (__cerb) 
	{
	  try 
	    {
	      __c = this->rdbuf()->sbumpc();
	      // 27.6.1.1 paragraph 3
	      if (!traits_type::eq_int_type(__c, __eof))
		_M_gcount = 1;
	      else
		this->setstate(ios_base::eofbit | ios_base::failbit);
	    }
	  catch(...)
	    {
	      // 27.6.1.3 paragraph 1
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return __c;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>&
    basic_istream<_CharT, _Traits>::
    get(char_type& __c)
    {
      _M_gcount = 0;
      sentry __cerb(*this, true);
      if (__cerb) 
	{
 	  try 
	    {
	      const int_type __eof = traits_type::eof();
	      int_type __bufval = this->rdbuf()->sbumpc();
	      // 27.6.1.1 paragraph 3
	      if (!traits_type::eq_int_type(__bufval, __eof))
		{
		  _M_gcount = 1;
		  __c = traits_type::to_char_type(__bufval);
		}
	      else
		this->setstate(ios_base::eofbit | ios_base::failbit);
	    }
	  catch(...)
	    {
	      // 27.6.1.3 paragraph 1
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>&
    basic_istream<_CharT, _Traits>::
    get(char_type* __s, streamsize __n, char_type __delim)
    {
      _M_gcount = 0;
      sentry __cerb(*this, true);
      if (__cerb) 
	{
	  try 
	    {
	      const int_type __idelim = traits_type::to_int_type(__delim);
	      const int_type __eof = traits_type::eof();
	      __streambuf_type* __sb = this->rdbuf();
	      int_type __c = __sb->sgetc();	
	      
	      while (_M_gcount + 1 < __n 
		     && !traits_type::eq_int_type(__c, __eof)
		     && !traits_type::eq_int_type(__c, __idelim))
		{
		  *__s++ = traits_type::to_char_type(__c);
		  __c = __sb->snextc();
		  ++_M_gcount;
		}
	      if (traits_type::eq_int_type(__c, __eof))
		this->setstate(ios_base::eofbit);
	    }
	  catch(...)
	    {
	      // 27.6.1.3 paragraph 1
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      *__s = char_type();
      if (!_M_gcount)
	this->setstate(ios_base::failbit);
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>&
    basic_istream<_CharT, _Traits>::
    get(__streambuf_type& __sb, char_type __delim)
    {
      _M_gcount = 0;
      sentry __cerb(*this, true);
      if (__cerb) 
	{
	  try 
	    {
	      const int_type __idelim = traits_type::to_int_type(__delim);
	      const int_type __eof = traits_type::eof();	      
	      __streambuf_type* __this_sb = this->rdbuf();
	      int_type __c = __this_sb->sgetc();
	      char_type __c2 = traits_type::to_char_type(__c);
	      
	      while (!traits_type::eq_int_type(__c, __eof) 
		     && !traits_type::eq_int_type(__c, __idelim) 
		     && !traits_type::eq_int_type(__sb.sputc(__c2), __eof))
		{
		  ++_M_gcount;
		  __c = __this_sb->snextc();
		  __c2 = traits_type::to_char_type(__c);
		}
	      if (traits_type::eq_int_type(__c, __eof))
		this->setstate(ios_base::eofbit);
	    }
	  catch(...)
	    {
	      // 27.6.1.3 paragraph 1
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      if (!_M_gcount)
	this->setstate(ios_base::failbit);
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>&
    basic_istream<_CharT, _Traits>::
    getline(char_type* __s, streamsize __n, char_type __delim)
    {
      _M_gcount = 0;
      sentry __cerb(*this, true);
      if (__cerb) 
	{
          try 
	    {
	      const int_type __idelim = traits_type::to_int_type(__delim);
	      const int_type __eof = traits_type::eof();
	      __streambuf_type* __sb = this->rdbuf();
	      int_type __c = __sb->sgetc();
	    
	      while (_M_gcount + 1 < __n 
		     && !traits_type::eq_int_type(__c, __eof)
		     && !traits_type::eq_int_type(__c, __idelim))
		{
		  *__s++ = traits_type::to_char_type(__c);
		  __c = __sb->snextc();
		  ++_M_gcount;
		}
	      if (traits_type::eq_int_type(__c, __eof))
		this->setstate(ios_base::eofbit);
	      else
		{
		  if (traits_type::eq_int_type(__c, __idelim))
		    {
		      __sb->sbumpc();
		      ++_M_gcount;
		    }
		  else
		    this->setstate(ios_base::failbit);
		}
	    }
	  catch(...)
	    {
	      // 27.6.1.3 paragraph 1
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      *__s = char_type();
      if (!_M_gcount)
	this->setstate(ios_base::failbit);
      return *this;
    }
  
  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>&
    basic_istream<_CharT, _Traits>::
    ignore(streamsize __n, int_type __delim)
    {
      _M_gcount = 0;
      sentry __cerb(*this, true);
      if (__cerb && __n > 0) 
	{
	  try 
	    {
	      const int_type __eof = traits_type::eof();
	      __streambuf_type* __sb = this->rdbuf();
	      int_type __c;
	      
	      __n = min(__n, numeric_limits<streamsize>::max());
	      while (_M_gcount < __n  
		     && !traits_type::eq_int_type(__c = __sb->sbumpc(), __eof))
		{
		  ++_M_gcount;
		  if (traits_type::eq_int_type(__c, __delim))
		    break;
		}
	      if (traits_type::eq_int_type(__c, __eof))
		this->setstate(ios_base::eofbit);
	    }
	  catch(...)
	    {
	      // 27.6.1.3 paragraph 1
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return *this;
    }
  
  template<typename _CharT, typename _Traits>
    typename basic_istream<_CharT, _Traits>::int_type
    basic_istream<_CharT, _Traits>::
    peek(void)
    {
      int_type __c = traits_type::eof();
      _M_gcount = 0;
      sentry __cerb(*this, true);
      if (__cerb)
	{
	  try 
	    { __c = this->rdbuf()->sgetc(); }
	  catch(...)
	    {
	      // 27.6.1.3 paragraph 1
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	} 
      return __c;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>&
    basic_istream<_CharT, _Traits>::
    read(char_type* __s, streamsize __n)
    {
      _M_gcount = 0;
      sentry __cerb(*this, true);
      if (__cerb) 
	{
	  try 
	    {
	      _M_gcount = this->rdbuf()->sgetn(__s, __n);
	      if (_M_gcount != __n)
		this->setstate(ios_base::eofbit | ios_base::failbit);
	    }	    
	  catch(...)
	    {
	      // 27.6.1.3 paragraph 1
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      else
	this->setstate(ios_base::failbit);
      return *this;
    }
  
  template<typename _CharT, typename _Traits>
    streamsize 
    basic_istream<_CharT, _Traits>::
    readsome(char_type* __s, streamsize __n)
    {
      _M_gcount = 0;
      sentry __cerb(*this, true);
      if (__cerb) 
	{
	  try 
	    {
	      // Cannot compare int_type with streamsize generically.
	      streamsize __num = this->rdbuf()->in_avail();
	      if (__num >= 0)
		{
		  __num = min(__num, __n);
		  if (__num)
		    _M_gcount = this->rdbuf()->sgetn(__s, __num);
		}
	      else
		this->setstate(ios_base::eofbit);		    
	    }
	  catch(...)
	    {
	      // 27.6.1.3 paragraph 1
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      else
	this->setstate(ios_base::failbit);
      return _M_gcount;
    }
      
  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>&
    basic_istream<_CharT, _Traits>::
    putback(char_type __c)
    {
#ifdef _GLIBCPP_RESOLVE_LIB_DEFECTS
// 60. What is a formatted input function?
      _M_gcount = 0;
#endif
      sentry __cerb(*this, true);
      if (__cerb) 
	{
	  try 
	    {
	      const int_type __eof = traits_type::eof();
	      __streambuf_type* __sb = this->rdbuf();
	      if (!__sb 
		  || traits_type::eq_int_type(__sb->sputbackc(__c), __eof))
		this->setstate(ios_base::badbit);		    
	    }
	  catch(...)
	    {
	      // 27.6.1.3 paragraph 1
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      else
	this->setstate(ios_base::failbit);
      return *this;
    }
  
  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>&
    basic_istream<_CharT, _Traits>::
    unget(void)
    {
#ifdef _GLIBCPP_RESOLVE_LIB_DEFECTS
// 60. What is a formatted input function?
      _M_gcount = 0;
#endif
      sentry __cerb(*this, true);
      if (__cerb) 
	{
	  try 
	    {
	      const int_type __eof = traits_type::eof();
	      __streambuf_type* __sb = this->rdbuf();
	      if (!__sb 
		  || traits_type::eq_int_type(__sb->sungetc(), __eof))
		this->setstate(ios_base::badbit);		    
	    }
	  catch(...)
	    {
	      // 27.6.1.3 paragraph 1
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      else
	this->setstate(ios_base::failbit);
      return *this;
    }
  
  template<typename _CharT, typename _Traits>
    int
    basic_istream<_CharT, _Traits>::
    sync(void)
    {
      // DR60.  Do not change _M_gcount.
      int __ret = -1;
      sentry __cerb(*this, true);
      if (__cerb) 
	{
	  try 
	    {
	      __streambuf_type* __sb = this->rdbuf();
	      if (__sb)
		{
		  if (__sb->pubsync() == -1)
		    this->setstate(ios_base::badbit);		    
		  else 
		    __ret = 0;
		}
	    }
	  catch(...)
	    {
	      // 27.6.1.3 paragraph 1
	      // Turn this on without causing an ios::failure to be thrown.
	      this->_M_setstate(ios_base::badbit);
	      if ((this->exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      return __ret;
    }
  
  template<typename _CharT, typename _Traits>
    typename basic_istream<_CharT, _Traits>::pos_type
    basic_istream<_CharT, _Traits>::
    tellg(void)
    {
      // DR60.  Do not change _M_gcount.
      pos_type __ret = pos_type(-1);
      if (!this->fail())
	__ret = this->rdbuf()->pubseekoff(0, ios_base::cur, ios_base::in);
      return __ret;
    }


  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>&
    basic_istream<_CharT, _Traits>::
    seekg(pos_type __pos)
    {
      // DR60.  Do not change _M_gcount.
      if (!this->fail())
	{
#ifdef _GLIBCPP_RESOLVE_LIB_DEFECTS
// 136.  seekp, seekg setting wrong streams?
	  pos_type __err = this->rdbuf()->pubseekpos(__pos, ios_base::in);

// 129. Need error indication from seekp() and seekg()
	  if (__err == pos_type(off_type(-1)))
	    this->setstate(ios_base::failbit);
#endif
	}
      return *this;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>&
    basic_istream<_CharT, _Traits>::
    seekg(off_type __off, ios_base::seekdir __dir)
    {
      // DR60.  Do not change _M_gcount.
      if (!this->fail())
	{
#ifdef _GLIBCPP_RESOLVE_LIB_DEFECTS
// 136.  seekp, seekg setting wrong streams?
	  pos_type __err = this->rdbuf()->pubseekoff(__off, __dir, 
						     ios_base::in);

// 129. Need error indication from seekp() and seekg()
	  if (__err == pos_type(off_type(-1)))
	    this->setstate(ios_base::failbit);
#endif
	}
      return *this;
    }

  // 27.6.1.2.3 Character extraction templates
  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>&
    operator>>(basic_istream<_CharT, _Traits>& __in, _CharT& __c)
    {
      typedef basic_istream<_CharT, _Traits> 		__istream_type;
      typename __istream_type::sentry __cerb(__in, false);
      if (__cerb)
	{
	  try 
	    { __in.get(__c); }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      __in._M_setstate(ios_base::badbit);
	      if ((__in.exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      else
	__in.setstate(ios_base::failbit);
      return __in;
    }

  template<typename _CharT, typename _Traits>
    basic_istream<_CharT, _Traits>&
    operator>>(basic_istream<_CharT, _Traits>& __in, _CharT* __s)
    {
      typedef basic_istream<_CharT, _Traits> 		__istream_type;
      typedef typename __istream_type::__streambuf_type __streambuf_type;
      typedef typename _Traits::int_type 		int_type;
      typedef _CharT                     		char_type;
      typedef ctype<_CharT>     			__ctype_type;
      streamsize __extracted = 0;

      typename __istream_type::sentry __cerb(__in, false);
      if (__cerb)
	{
	  try 
	    {
	      // Figure out how many characters to extract.
	      streamsize __num = __in.width();
	      if (__num == 0)
		__num = numeric_limits<streamsize>::max();
	      
	      const __ctype_type& __ctype = use_facet<__ctype_type>(__in.getloc());
	      const int_type __eof = _Traits::eof();
	      __streambuf_type* __sb = __in.rdbuf();
	      int_type __c = __sb->sgetc();
	      
	      while (__extracted < __num - 1 
		     && !_Traits::eq_int_type(__c, __eof)
		     && !__ctype.is(ctype_base::space, _Traits::to_char_type(__c)))
		{
		  *__s++ = _Traits::to_char_type(__c);
		  ++__extracted;
		  __c = __sb->snextc();
		}
	      if (_Traits::eq_int_type(__c, __eof))
		__in.setstate(ios_base::eofbit);

#ifdef _GLIBCPP_RESOLVE_LIB_DEFECTS
//68.  Extractors for char* should store null at end
	      *__s = char_type();
#endif
	      __in.width(0);
	    }
	  catch(...)
	    {
	      // 27.6.1.2.1 Common requirements.
	      // Turn this on without causing an ios::failure to be thrown.
	      __in._M_setstate(ios_base::badbit);
	      if ((__in.exceptions() & ios_base::badbit) != 0)
		__throw_exception_again;
	    }
	}
      if (!__extracted)
	__in.setstate(ios_base::failbit);
      return __in;
    }

  // 27.6.1.4 Standard basic_istream manipulators
  template<typename _CharT, typename _Traits>
    basic_istream<_CharT,_Traits>& 
    ws(basic_istream<_CharT,_Traits>& __in)
    {
      typedef basic_istream<_CharT, _Traits> 		__istream_type;
      typedef typename __istream_type::__streambuf_type __streambuf_type;
      typedef typename __istream_type::__ctype_type 	__ctype_type;
      typedef typename __istream_type::int_type 	__int_type;

      const __ctype_type& __ctype = use_facet<__ctype_type>(__in.getloc());
      const __int_type __eof = _Traits::eof();	      
      __streambuf_type* __sb = __in.rdbuf();
      __int_type __c = __sb->sgetc();

      while (!_Traits::eq_int_type(__c, __eof) 
	     && __ctype.is(ctype_base::space, _Traits::to_char_type(__c)))
	__c = __sb->snextc();

       if (_Traits::eq_int_type(__c, __eof))
	__in.setstate(ios_base::eofbit);

      return __in;
    }

  // 21.3.7.9 basic_string::getline and operators
  template<typename _CharT, typename _Traits, typename _Alloc>
    basic_istream<_CharT, _Traits>&
    operator>>(basic_istream<_CharT, _Traits>& __in,
	       basic_string<_CharT, _Traits, _Alloc>& __str)
    {
      typedef basic_istream<_CharT, _Traits> 		__istream_type;
      typedef typename __istream_type::int_type 	__int_type;
      typedef typename __istream_type::__streambuf_type __streambuf_type;
      typedef typename __istream_type::__ctype_type 	__ctype_type;
      typedef basic_string<_CharT, _Traits, _Alloc> 	__string_type;
      typedef typename __string_type::size_type		__size_type;
      __size_type __extracted = 0;

      typename __istream_type::sentry __cerb(__in, false);
      if (__cerb) 
	{
	  __str.erase();
	  streamsize __w = __in.width();
	  __size_type __n;
	  __n = __w > 0 ? static_cast<__size_type>(__w) : __str.max_size();

	  const __ctype_type& __ctype = use_facet<__ctype_type>(__in.getloc());
	  const __int_type __eof = _Traits::eof();
	  __streambuf_type* __sb = __in.rdbuf();
	  __int_type __c = __sb->sgetc();
	  
	  while (__extracted < __n 
		 && !_Traits::eq_int_type(__c, __eof)
		 && !__ctype.is(ctype_base::space, _Traits::to_char_type(__c)))
	    {
	      __str += _Traits::to_char_type(__c);
	      ++__extracted;
	      __c = __sb->snextc();
	    }
	  if (_Traits::eq_int_type(__c, __eof))
	    __in.setstate(ios_base::eofbit);
	  __in.width(0);
	}
#ifdef _GLIBCPP_RESOLVE_LIB_DEFECTS
//211.  operator>>(istream&, string&) doesn't set failbit
      if (!__extracted)
	__in.setstate (ios_base::failbit);
#endif
      return __in;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    basic_istream<_CharT, _Traits>&
    getline(basic_istream<_CharT, _Traits>& __in,
	    basic_string<_CharT, _Traits, _Alloc>& __str, _CharT __delim)
    {
      typedef basic_istream<_CharT, _Traits> 		__istream_type;
      typedef typename __istream_type::int_type 	__int_type;
      typedef typename __istream_type::__streambuf_type __streambuf_type;
      typedef typename __istream_type::__ctype_type 	__ctype_type;
      typedef basic_string<_CharT, _Traits, _Alloc> 	__string_type;
      typedef typename __string_type::size_type		__size_type;

      __size_type __extracted = 0;
      bool __testdelim = false;
      typename __istream_type::sentry __cerb(__in, true);
      if (__cerb) 
	{
	  __str.erase();
	  __size_type __n = __str.max_size();

	  __int_type __idelim = _Traits::to_int_type(__delim);
	  __streambuf_type* __sb = __in.rdbuf();
	  __int_type __c = __sb->sbumpc();
	  const __int_type __eof = _Traits::eof();
	  __testdelim = _Traits::eq_int_type(__c, __idelim);

	  while (__extracted <= __n 
		 && !_Traits::eq_int_type(__c, __eof)
		 && !__testdelim)
	    {
	      __str += _Traits::to_char_type(__c);
	      ++__extracted;
	      __c = __sb->sbumpc();
	      __testdelim = _Traits::eq_int_type(__c, __idelim);
	    }
	  if (_Traits::eq_int_type(__c, __eof))
	    __in.setstate(ios_base::eofbit);
	}
      if (!__extracted && !__testdelim)
	__in.setstate(ios_base::failbit);
      return __in;
    }

  template<class _CharT, class _Traits, class _Alloc>
    inline basic_istream<_CharT,_Traits>&
    getline(basic_istream<_CharT, _Traits>& __in, 
	    basic_string<_CharT,_Traits,_Alloc>& __str)
    { return getline(__in, __str, __in.widen('\n')); }

  // Inhibit implicit instantiations for required instantiations,
  // which are defined via explicit instantiations elsewhere.  
  // NB:  This syntax is a GNU extension.
#if _GLIBCPP_EXTERN_TEMPLATE
  extern template class basic_istream<char>;
  extern template istream& ws(istream&);
  extern template istream& operator>>(istream&, char&);
  extern template istream& operator>>(istream&, char*);
  extern template istream& operator>>(istream&, unsigned char&);
  extern template istream& operator>>(istream&, signed char&);
  extern template istream& operator>>(istream&, unsigned char*);
  extern template istream& operator>>(istream&, signed char*);

#ifdef _GLIBCPP_USE_WCHAR_T
  extern template class basic_istream<wchar_t>;
  extern template wistream& ws(wistream&);
  extern template wistream& operator>>(wistream&, wchar_t&);
  extern template wistream& operator>>(wistream&, wchar_t*);
#endif
#endif
} // namespace std
