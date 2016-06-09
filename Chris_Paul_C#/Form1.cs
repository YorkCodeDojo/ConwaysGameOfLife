using System;
using System.Collections.Generic;
using System.Drawing;
using System.Windows.Forms;
using System.Threading;

namespace Circle
{
    public partial class Form1 : Form
    {
        DateTime _lastCheckTime = DateTime.Now;
        int _frameCount = 0;
        Font drawFont;
        SolidBrush drawBrush;
        PointF drawPoint;
       
        public Form1()
        {
            InitializeComponent();

            // Create font and brush.
            drawFont = new Font("Arial", 14);
            drawBrush = new SolidBrush(Color.White);

            // Create point for upper-left corner of drawing.
            drawPoint = new PointF(5.0F, 5.0F);

            this.DoubleBuffered = true;

            StartDrawing();
            this.KeyUp += Form1_KeyUp;



        }

        // called every once in a while
        double GetFps()
        {
            double secondsElapsed = (DateTime.Now - _lastCheckTime).TotalSeconds;
            long count = Interlocked.Exchange(ref _frameCount, 0);
            double fps = count / secondsElapsed;
            _lastCheckTime = DateTime.Now;
            _frameCount++;
            return fps;
        }

        int scale = 1;
        bool reset = false;
        private void Form1_KeyUp(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Add || e.KeyCode == Keys.Oemplus)
            {
                scale++;
                reset = true;
            }
            else if (e.KeyCode == Keys.Subtract || e.KeyCode == Keys.OemMinus)
            {
                scale--;
                reset = true;
            }
            if (scale < 1)
            {
                scale = 1;
                reset = false;
            }
            if (scale > 10)
            {
                scale = 10;
                reset = false;
            }
            
        }

        System.Windows.Forms.Timer _timer;

        public void StartDrawing()
        {
            _timer = new System.Windows.Forms.Timer();
            _timer.Interval = 1;
            _timer.Tick += Redraw;
            _timer.Enabled = true;
        }

        private void Redraw(object sender, EventArgs e)
        {
            
            this.Invalidate();
        }

        byte[][] arrays = null;
        int currentReadBuffer = 0;
        Bitmap bmp = null;
        const int byteDepth = 4;

        protected override void OnPaintBackground(PaintEventArgs pevent)
        {
        }
 
        
        private unsafe void Form1_Paint(object sender, PaintEventArgs e)
        {
            var g = e.Graphics;
            g.CompositingQuality = System.Drawing.Drawing2D.CompositingQuality.Default;
            g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.NearestNeighbor;

            int w = this.Width / scale;
            int h = this.Height / scale;
            if (reset || bmp == null ||
                bmp.Width != w ||
                bmp.Height != h)
            {
                reset = false;
                bmp = new Bitmap(w, h, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
                arrays = new byte[2][];
                arrays[0] = new byte[w * h];
                arrays[1] = new byte[w * h];

                var rand = new Random();
                currentReadBuffer = 0;
                for (int y = 0; y < h; y++)
                {
                    for (int x = 0; x < w; x++)
                    {
                        arrays[currentReadBuffer][x + (y * w)] = (byte)rand.Next(2);
                    }
                }
            }
            
            var readBuffer = arrays[currentReadBuffer];
            var writeBuffer = arrays[currentReadBuffer == 0 ? 1 : 0];
            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    int numNeighbours = 0;
                    for (int offsetX = -1; offsetX <= 1; offsetX++)
                    {
                        for (int offsetY = -1; offsetY <= 1; offsetY++)
                        {
                            if (offsetX == 0 && offsetY == 0)
                            {
                                continue;
                            }

                            int newX = x + offsetX;
                            int newY = y + offsetY;
                            if (newX < 0) newX = w - newX;
                            if (newY < 0) newY = h - newY;
                            if (newX >= w) newX = newX - w;
                            if (newY >= h) newY = newY - h;
                            var test = readBuffer[newX + (newY * w)];
                            if (test == 1)
                            {
                                numNeighbours++;
                            }
                        }

                        if (readBuffer[x + (y * w)] == 1)
                        {
                            if (numNeighbours == 2 || numNeighbours == 3)
                            {
                                writeBuffer[x + (y * w)] = 1;
                            }
                            else
                            {
                                writeBuffer[x + (y * w)] = 0;
                            }
                        }
                        else
                        {
                            writeBuffer[x + (y * w)] = (byte)(numNeighbours == 3 ? 1 : 0);
                        }
                    }
                }
            }

            Rectangle rect = new Rectangle(0, 0, w, h);

            var lockData = bmp.LockBits(rect, System.Drawing.Imaging.ImageLockMode.WriteOnly, bmp.PixelFormat);

            var ptr = (byte*)lockData.Scan0.ToPointer();
            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    var pData = (UInt32*)(ptr + (y * lockData.Stride) + (x * 4));

                    if (readBuffer[x + (y * w)] == 1)
                    { 
                        *pData = 0xFFFFFFFF;
                    }
                    else
                    {
                        *pData = 0xFF000000;
                    }
                }
            }

            bmp.UnlockBits(lockData);

            var rc = new Rectangle(this.ClientRectangle.Left, this.ClientRectangle.Top + 30, this.ClientRectangle.Width, this.ClientRectangle.Height - 30);

            g.DrawImage(bmp, rc);
            currentReadBuffer = currentReadBuffer == 0 ? 1 : 0;

            g.DrawString(string.Format("Generations/Sec: {0}", (int)GetFps()), drawFont, drawBrush, drawPoint);
        }


    }
}
