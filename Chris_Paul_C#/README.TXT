Creates two byte array buffers and ping-pongs between them.
Shows Generations per second
Has +/- keys for scaling the simulation
We used an unsafe pointer for speed.

Runs about 40fps on my I7 Surface Pro 4, with default window size.

Issues:
- We used a timer for update, I seem to recall this is not the best way for an animated app.

Findings:
- Reading Bitmap.Width/Height killed performance
- Setting DoubleBuffered to true makes the rendering cleaner