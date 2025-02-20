﻿using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace EpicCommonUtils
{
    public partial class SlowProgressDialog : Form
    {
        public string ExceptionResult = null;

        public SlowProgressDialog()
        {
            InitializeComponent();
        }

        public delegate void WorkToDoSignature(BackgroundWorker BGWorker);
        public WorkToDoSignature OnBeginBackgroundWork = null;

        private void SlowWork_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            DialogResult = (ExceptionResult == null) ? DialogResult.OK : DialogResult.Abort;
        }

        private void SlowWork_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                OnBeginBackgroundWork(SlowWork);
            }
            catch (Exception ex)
            {
                ExceptionResult = ex.Message;
            }
        }

        private void SlowWork_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            ProgressIndicator.Value = Math.Min(Math.Max(ProgressIndicator.Minimum, e.ProgressPercentage), ProgressIndicator.Maximum);
            StatusLabel.Text = e.UserState as string;
        }

        private void SlowProgressDialog_Shown(object sender, EventArgs e)
        {
            if (OnBeginBackgroundWork != null)
            {
                ExceptionResult = null;
                SlowWork.RunWorkerAsync();
            }
            else
            {
                DialogResult = DialogResult.Abort;
            }
        }
    }
}
