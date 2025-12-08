This is a very efficient C Quadtree that does not use any recursion.

On my laptop at least, it handles 400,000 entities of wildly varying sizes at around 14.2mspt, with 1,000x 1920x1080 (roughly) queries taking around 0.27mspt. Here's a screenshot of the simulation:

![Screenshot of the simulation](simulation.png)

Made for my private use, but it doesn't take a genius to start using this by yourself.

Still in development. There are a lot of things that could be improved to increase performance.

If you want to run the simulation yourself, just execute `make`.
