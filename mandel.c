///
// mandel.c
// Based on example code found here:
// https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
//
// Converted to use jpg instead of BMP and other minor changes
//
// Modified by: Zach Campbell
// Course: CPE2600 Systems Programming
// Assignment: lab 12 - Multithreading
// Section: 121
///
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>

#include "jpegrw.h"
// local routines
static int iteration_to_color( int i, int max);
static int iterations_at_point( double x, double y, int max);

static int compute_image(imgRawImage *img, double xmin, double xmax,
double ymin, double ymax, int max, int num_threads);
static void *compute_region(void *arg);
static void show_help();
/* 
Stores the information required by each worker thread to calculate its assigned
portion of the Mandelbrot image 
*/
typedef struct
{
    imgRawImage *img;

    double xmin;
    double xmax;
    double ymin;
    double ymax;

    int max;

    int start_row;
    int end_row;

} ThreadData;

int main( int argc, char *argv[] )
{
    int num_threads = 1;
    int opt;
    // These are the default configuration values used
    // if no command line arguments are given.
    const char *outfile = "mandel.jpg";
    double xcenter = 0;
    double ycenter = 0;
    double xscale = 4;
    double yscale = 0; // calc later
    int image_width = 1000;
    int image_height = 1000;
    int max = 1000;
    // For each command line argument given,
    // override the appropriate configuration value.
    while((opt = getopt(argc,argv,"x:y:s:W:H:m:o:t:h"))!=-1) {
        switch(opt)
        {
            case 'x':
                xcenter = atof(optarg);
                break;
            case 'y':
                ycenter = atof(optarg);
                break;
            case 's':
                xscale = atof(optarg);
                break;
            case 'W':
                image_width = atoi(optarg);
                break;
            case 'H':
                image_height = atoi(optarg);
                break;
            case 'm':
                max = atoi(optarg);
                break;
            case 'o':
                outfile = optarg;
                break;
            case 't':
                char *endptr;
                long value;

                errno = 0;
                value = strtol(optarg, &endptr, 10);
                if (errno != 0 ||
                    endptr == optarg ||
                    *endptr != '\0' ||
                    value < 1 ||
                    value > INT_MAX)
                {
                    fprintf(stderr, "Invalid thread count: %s\n", optarg);
                    return EXIT_FAILURE;
                }

                num_threads = (int)value;
                break;
            case 'h':
                show_help();
                return EXIT_SUCCESS;
        }
    }
    // Validate image configuration
    if (image_width <= 0 ||
        image_height <= 0 ||
        max <= 0 || xscale <= 0)
        {
            fprintf(stderr, "Invalid image configuration\n");
            return EXIT_FAILURE;
        }
    // Calculate y scale based on x scale (settable) and image sizes in X and Y (setatable)
    yscale = xscale / image_width * image_height;
    // Display the configuration of the image.
    printf("mandel: x=%lf y=%lf xscale=%lf yscale=%1f max=%d threads=%d outfile=%s\n",xcenter,ycenter,xscale,yscale,max,num_threads,outfile);
    // Create a raw image of the appropriate size and validate
    imgRawImage* img = initRawImage(image_width,image_height);

    if (img == NULL)
    {
        fprintf(stderr, "Error allocating image memory\n");
        return EXIT_FAILURE;
    }
    // Fill it with a black
    setImageCOLOR(img,0);
    // Compute the Mandelbrot image
    if (compute_image(img,xcenter-xscale/2,xcenter+xscale/2,ycenter-yscale/
                  2,ycenter+yscale/2,max,num_threads) != 0)
    {
        fprintf(stderr, "Error computing Mandelbrot image\n");

        freeRawImage(img);
        return EXIT_FAILURE;
    }
    // Save the image in the stated file.
    storeJpegImageFile(img,outfile);
    // free the mallocs
    freeRawImage(img);
    return 0;
}
/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/
static int iterations_at_point( double x, double y, int max )
{
    double x0 = x;
    double y0 = y;
    int iter = 0;
    while( (x*x + y*y <= 4) && iter < max ) {
        double xt = x*x - y*y + x0;
        double yt = 2*x*y + y0;
        x = xt;     
        y = yt;
        iter++;
    }
    return iter;
}

/*
Worker function executed by each pthread, each thread calcualtes a seperate
range of image rows 
*/
static void *compute_region(void *arg)
{
    ThreadData *data = (ThreadData *)arg;

    unsigned int i;
    int j;

    double x;
    double y;

    int iterations;

    for (j = data->start_row; j < data->end_row; j++)
    {
        for (i = 0; i < data->img->width; i++)
        {
            x = data->xmin + (data->xmax - data->xmin) * i / data->img->width;

            y = data->ymin + (data->ymax - data->ymin) * j / data->img->height;

            iterations = iterations_at_point(x, y, data->max);

            setPixelCOLOR(data->img, i, j, iteration_to_color(iterations, data->max));
        }
    }

    return NULL;
}
/*
Compute an entire Mandelbrot image, using multiple threads.
The image is divided into horizontal regions.
Each thread calculates its assigned rows.
The main thread joins all worker threasd before returning
*/
static int compute_image(imgRawImage *img, double xmin, double xmax, double ymin, double
                   ymax, int max, int num_threads)
{
    pthread_t *threads;
    ThreadData *thread_data;
    int i;
    int created_threads = 0;
    int join_error = 0;
    /*
    Dynamicall allocate space for the requested number of threads and 
    their individual argument structures
    */

    threads = malloc(sizeof(pthread_t) * (size_t)num_threads);
    thread_data = malloc(sizeof(ThreadData) * (size_t)num_threads);

    if (threads == NULL || thread_data == NULL)
    {
        fprintf(stderr, "Error allocating thread memory\n");

        free(threads);
        free(thread_data);

        return -1;
    }

    /*
    Divide the image into approximately equal row ranges.
    Integer division distrubtes any remaining rows
    */
    for (i = 0; i < num_threads; i++)
    {
        thread_data[i].img = img;

        thread_data[i].xmin = xmin;
        thread_data[i].xmax = xmax;
        thread_data[i].ymin = ymin;
        thread_data[i].ymax = ymax;

        thread_data[i].max = max;

        thread_data[i].start_row = (int)((long long)i * img->height / num_threads);

        thread_data[i].end_row = (int)((long long)(i + 1) * img->height / num_threads);

        // Create the thread using its corresponding argument structure
        int result = pthread_create(&threads[i], NULL, compute_region, &thread_data[i]);
        if (result != 0)
        {
            fprintf(stderr, "Error creating thread %d: %s\n", i, strerror(result));

            break;
        }

        created_threads++;
    }

    /* 
    Wait for every successful created thread.
    Do not free the argument structures before joining
    */
    for (i = 0; i < created_threads; i++)
    {
        int result = pthread_join(threads[i], NULL);

        if (result != 0)
        {
            fprintf(stderr, "Error joining thread %d: %s\n", i, strerror(result));

            join_error = 1;
        }
    }

    free(threads);
    free(thread_data);

    if (created_threads != num_threads || join_error)
    {
        return -1;
    }

    return 0;
}
/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
static int iteration_to_color( int iters, int max )
{
    int color = 0xFFFFFF*iters/(double)max;
    return color;
}
// Show help message
static void show_help()
{
    printf("Use: mandel [options]\n");
    printf("Where options are:\n");
    printf("-m <max> The maximum number of iterations per point. (default=1000)\n");
    printf("-x <coord> X coordinate of image center point. (default=0)\n");
    printf("-y <coord> Y coordinate of image center point. (default=0)\n");
    printf("-s <scale> Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
    printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
    printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
    printf("-o <file> Output JPEG file. (default=mandel.jpg)\n");
    printf("-h Show this help text.\n");
    printf("\nSome examples are:\n");
    printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
    printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
    printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
    printf("-t <threads> Number of threads to use (default=1)\n");
}