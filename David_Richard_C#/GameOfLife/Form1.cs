using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace GameOfLife
{
    public partial class Form1 : Form
    {
        private const int numberOfRows = 100;
        private const int numberOfColumns = 100;
        private const int width = 30;
        private const int height = 30;

        private bool[,] cells;


        private bool[,] lineV = {
                                    { false, false, false },
                                    { false, true, false },
                                    { false, true, false },
                                    { false, true, false },
                                    { false, false, false }
                                };


        private bool[,] lineH = {
                                    { false, false, false, false, false },
                                    { false, true, true, true, false },
                                    { false, false, false, false , false}
                                };


        private bool[,] box = {
                                    { false, false, false, false},
                                    { false, true, true, false },
                                    { false, true, true, false },
                                    { false, false, false, false }
                                };

        public Form1()
        {
            InitializeComponent();

            cells = new bool[numberOfRows, numberOfColumns];

            var random = new Random();
            for (int rowNumber = 0; rowNumber < numberOfRows; rowNumber++)
            {
                for (int columnNumber = 0; columnNumber < numberOfColumns; columnNumber++)
                {
                    cells[rowNumber, columnNumber] = random.Next(2) == 1; ;
                }
            }
            //cells[10, 10] = true;
            //cells[11, 11] = true;
            //cells[12, 11] = true;
            //cells[12, 10] = true;
            //cells[12, 9] = true;

            this.WindowState = FormWindowState.Maximized;

        }

        private void IdentifyShapes()
        {
            FindShape(lineV);
            FindShape(lineH);
            FindShape(box);
        }

        private void FindShape(bool[,] shapeToFind)
        {
            for (int rowNumber = 0; rowNumber < numberOfRows - 1 - shapeToFind.GetUpperBound(1); rowNumber++)
            {
                for (int columnNumber = 0; columnNumber < numberOfColumns - 1 - shapeToFind.GetUpperBound(0); columnNumber++)
                {


                    var matches = true;  //Assume the best
                    for (int x = 0; x < shapeToFind.GetUpperBound(0); x++)
                    {
                        for (int y = 0; y < shapeToFind.GetUpperBound(1); y++)
                        {
                            if (cells[rowNumber + x, columnNumber + y] != shapeToFind[x, y])
                            {
                                matches = false;
                                break;
                            }
                        }
                        if (!matches) break;
                    }

                    if (matches)
                    {
                        var g = this.CreateGraphics();
                        for (int x = 0; x < shapeToFind.GetUpperBound(0); x++)
                        {
                            for (int y = 0; y < shapeToFind.GetUpperBound(1); y++)
                            {
                                if (cells[rowNumber + x, columnNumber + y])
                                {
                                    g.FillRectangle(new SolidBrush(Color.Red), (columnNumber + y) * width, (rowNumber + x) * height, width, height);
                                }

                            }
                        }
                    }


                }
            }
        }

        private void NextGeneration()
        {
            var nextCells = new bool[numberOfRows, numberOfColumns];

            for (int rowNumber = 0; rowNumber < numberOfRows; rowNumber++)
            {
                for (int columnNumber = 0; columnNumber < numberOfColumns; columnNumber++)
                {
                    //COunt the number of neighbours
                    var numberOfNeighbours = 0;
                    if (rowNumber > 0 && columnNumber > 0 && cells[rowNumber - 1, columnNumber - 1]) numberOfNeighbours++;
                    if (rowNumber > 0 && cells[rowNumber - 1, columnNumber]) numberOfNeighbours++;
                    if (rowNumber > 0 && columnNumber < (numberOfColumns - 1) && cells[rowNumber - 1, columnNumber + 1]) numberOfNeighbours++;

                    if (columnNumber > 0 && cells[rowNumber, columnNumber - 1]) numberOfNeighbours++;
                    if (columnNumber < (numberOfColumns - 1) && cells[rowNumber, columnNumber + 1]) numberOfNeighbours++;

                    if (rowNumber < numberOfRows - 1 && columnNumber > 0 && cells[rowNumber + 1, columnNumber - 1]) numberOfNeighbours++;
                    if (rowNumber < numberOfRows - 1 && cells[rowNumber + 1, columnNumber]) numberOfNeighbours++;
                    if (rowNumber < numberOfRows - 1 && columnNumber < (numberOfColumns - 1) && cells[rowNumber + 1, columnNumber + 1]) numberOfNeighbours++;

                    //Rules
                    if (cells[rowNumber, columnNumber] && numberOfNeighbours < 2)
                        nextCells[rowNumber, columnNumber] = false;
                    else if (cells[rowNumber, columnNumber] && (numberOfNeighbours == 2 || numberOfNeighbours == 3))
                        nextCells[rowNumber, columnNumber] = true;
                    else if (cells[rowNumber, columnNumber] && (numberOfNeighbours > 3))
                        nextCells[rowNumber, columnNumber] = false;
                    else if ((!cells[rowNumber, columnNumber]) && (numberOfNeighbours == 3))
                        nextCells[rowNumber, columnNumber] = true;
                }
            }

            cells = nextCells;

        }


        private void timer1_Tick(object sender, EventArgs e)
        {
            var g = this.CreateGraphics();
            this.Refresh();


            for (int rowNumber = 0; rowNumber < numberOfRows; rowNumber++)
            {
                for (int columnNumber = 0; columnNumber < numberOfColumns; columnNumber++)
                {
                    if (cells[rowNumber, columnNumber])
                        g.FillRectangle(new SolidBrush(Color.Black), columnNumber * width, rowNumber * height, width, height);
                    else
                        g.FillRectangle(new SolidBrush(Color.White), columnNumber * width, rowNumber * height, width, height);

                }
            }

            NextGeneration();
            IdentifyShapes();
        }


    }
}
