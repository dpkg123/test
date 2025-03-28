﻿namespace EpicCommonUtils
{
    partial class SlowProgressDialog
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.SlowWork = new System.ComponentModel.BackgroundWorker();
            this.ProgressIndicator = new System.Windows.Forms.ProgressBar();
            this.StatusLabel = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // SlowWork
            // 
            this.SlowWork.WorkerReportsProgress = true;
            this.SlowWork.DoWork += new System.ComponentModel.DoWorkEventHandler(this.SlowWork_DoWork);
            this.SlowWork.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.SlowWork_RunWorkerCompleted);
            this.SlowWork.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.SlowWork_ProgressChanged);
            // 
            // ProgressIndicator
            // 
            this.ProgressIndicator.Location = new System.Drawing.Point(12, 12);
            this.ProgressIndicator.Name = "ProgressIndicator";
            this.ProgressIndicator.Size = new System.Drawing.Size(358, 23);
            this.ProgressIndicator.TabIndex = 0;
            // 
            // StatusLabel
            // 
            this.StatusLabel.AutoSize = true;
            this.StatusLabel.Location = new System.Drawing.Point(12, 53);
            this.StatusLabel.Name = "StatusLabel";
            this.StatusLabel.Size = new System.Drawing.Size(10, 13);
            this.StatusLabel.TabIndex = 1;
            this.StatusLabel.Text = " ";
            // 
            // SlowProgressDialog
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(382, 79);
            this.Controls.Add(this.StatusLabel);
            this.Controls.Add(this.ProgressIndicator);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "SlowProgressDialog";
            this.ShowIcon = false;
            this.ShowInTaskbar = false;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Processing...";
            this.Shown += new System.EventHandler(this.SlowProgressDialog_Shown);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.ComponentModel.BackgroundWorker SlowWork;
        private System.Windows.Forms.ProgressBar ProgressIndicator;
        private System.Windows.Forms.Label StatusLabel;
    }
}