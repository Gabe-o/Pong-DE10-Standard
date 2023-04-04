// address_map_arm.h
#define KEY_BASE 0xFF200050
#define HEX3_HEX0_BASE 0xFF200020
#define HEX5_HEX4_BASE 0xFF200030
#define ADC_BASE 0xFF204000
#define HPS_GPIO1_BASE 0xFF709000
#define HPS_RSTMGR_PREMODRST 0xFFD05014
#define SPIM0_BASE 0xFFF00000
#define SPIM0_SR 0xFFF00028
#define SPIM0_DR 0xFFF00060
#define TIMER_A9_BASE 0xFFFEC600

#define Mask_12_bits 0x00000FFF

volatile int *btnHardware = (int *)KEY_BASE;
volatile int *ADC_BASE_ptr = (int *)ADC_BASE;
volatile int adc1, adc2;

typedef struct _a9_timer
{
    int load;
    int count;
    int control;
    int status;
} a9_timer;
volatile a9_timer *const timer_1 = (a9_timer *)TIMER_A9_BASE;

// lcd_driver.c
void spim_write(int data)
{
    volatile int *spim0_sr = (int *)SPIM0_SR;
    volatile int *spim0_dr = (int *)SPIM0_DR;
    while (((*spim0_sr) & 0x4) != 0x4)
        ; // check status reg for empty
    (*spim0_dr) = data;
    while (((*spim0_sr) & 0x4) != 0x4)
        ; // check fifo is empty
    while (((*spim0_sr) & 0x1) != 0x0)
        ; // check spim has completed the transfer
}

void init_spim0(void)
{
    volatile int *rstmgr_premodrst = (int *)HPS_RSTMGR_PREMODRST;
    volatile int *spim0 = (int *)SPIM0_BASE;
    // Take SPIM0 out of reset
    *rstmgr_premodrst = *rstmgr_premodrst & (~0x00040000);
    // Turn SPIM0 OFF
    *(spim0 + 2) = 0x00000000;
    // Put SPIM0 in Tx Only Mode
    *(spim0 + 0) = *(spim0 + 0) & ~0x00000300;
    *(spim0 + 0) = *(spim0 + 0) | 0x00000100;
    // Set SPIM0 BAUD RATE
    *(spim0 + 5) = 0x00000040;
    // Set SPIM0 Slave Enable Register
    *(spim0 + 4) = 0x00000001;
    // Turn off interrupts
    *(spim0 + 11) = 0x00000000;
    // Turn SPIM0 ON
    *(spim0 + 2) = 0x00000001;
}

void init_lcd(void)
{
    volatile int *gpio1 = (int *)HPS_GPIO1_BASE;
    // Set GPIO1’s direction register for the outputs to the LCD
    *(gpio1 + 1) = *(gpio1 + 1) | 0x00009100;
    // Turn on the LCD Backlight and take it out of reset
    *(gpio1) = 0x00008100;
    // Initialize LCD’s registers
    spim_write(0x000000C8);
    spim_write(0x0000002F);
    spim_write(0x00000040);
    spim_write(0x000000B0);
    spim_write(0x00000000);
    spim_write(0x00000010);
    spim_write(0x000000AF);
}

/*
 * Sets the mode of GPIO1.
 *
 * mode: 1 for command mode, 0 for data mode
 */
void set_mode(int mode)
{
    volatile int *gpio1 = (int *)HPS_GPIO1_BASE;
    if (mode) // Enter command mode
        *(gpio1) = (*gpio1) & (~0x00001000);
    else // Enter data mode
        *(gpio1) = (*gpio1) | (0x00001000);
}

// lcd_graphic.c
#define FRAME_WIDTH 128
#define FRAME_HEIGHT 8
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

char frame_buffer[8][128];

/*
 * Writes the contents of the frame buffer to the LCD display.
 */
void refresh_buffer(void)
{
    int i, j;
    for (i = 0; i < FRAME_HEIGHT; i++)
    {
        set_mode(1);
        // Set page address
        spim_write(0x00B0 | i);
        // Set column address
        spim_write(0x0000);
        spim_write(0x0010);
        set_mode(0);
        for (j = 0; j < FRAME_WIDTH; j++)
            spim_write(frame_buffer[i][j]);
    }
}

/*
 * Clears the entire LCD display.
 */
void clear_screen(void)
{
    int i, j;
    for (i = 0; i < FRAME_HEIGHT; i++)
    {
        for (j = 0; j < FRAME_WIDTH; j++)
        {
            frame_buffer[i][j] = 0;
        }
    }
    refresh_buffer();
}

/*
 * Draws a line starting at (x, y) with the given color to (x, y+length) if
 * vert, else to (x+length, y) to the frame buffer.
 *
 * x: x coordinate of line start.
 * y: y coordinate of line start.
 * length: length of line.
 * color: color of line (0 for white, 1 for black).
 * vert: orientation of line (0 for horizontal, 1 for vertical).
 */
void LCD_line(int x, int y, int length, int color, int vert)
{
    int x_start, x_end, y_start, y_end;
    int i, page;
    char mask;
    if (vert)
    {
        y_start = y;
        y_end = y + length;
        for (i = y_start; i < y_end; i++)
        {
            page = i >> 3; // y/8
            mask = 0x01 << (i % 8);
            if (color)
                frame_buffer[page][x] |= mask;
            else
                frame_buffer[page][x] &= ~mask;
        }
    }
    else
    {
        x_start = x;
        x_end = x + length;
        page = y >> 3; // y/8
        mask = 0x01 << (y % 8);
        for (i = x_start; i < x_end; i++)
        {
            if (color)
                frame_buffer[page][i] |= mask;
            else
                frame_buffer[page][i] &= ~mask;
        }
    }
}

/*
 * Draws a width x height rectangle with the top left corner at (x, y) to the
 * frame buffer.
 *
 * x1: x coordinate of top left corner.
 * y1: y coordinate of top left corner.
 * width: width of rectangle.
 * height: height of rectangle.
 * color: color of rectangle (o for white, 1 for black).
 * fill: 1 if rectangle should be filled in, 0 to only draw rectangle outline.
 */
void LCD_rect(int x1, int y1, int width, int height, int color, int fill)
{
    int x2 = x1 + width;
    int y2 = y1 + width;
    int i;
    if (!fill)
    {
        LCD_line(x1, y1, width, color, 0);
        LCD_line(x1, y2, width, color, 0);
        LCD_line(x1, y1, height, color, 1);
        LCD_line(x2, y1, height, color, 1);
    }
    else
    {
        for (i = y1; i <= y2; i++)
            LCD_line(x1, i, width, color, 0);
    }
}

// pong
typedef struct ball_s
{
    int x, y;   /* position on the screen */
    int w, h;   // ball width and height
    int dx, dy; /* movement vector */
} ball_t;

typedef struct paddle
{
    int x, y;
    int w, h;
} paddle_t;

// Program globals
static ball_t ball;
static paddle_t paddle[2];
int score[] = {0, 0};

// init starting position and sizes of game elemements
static void init_game()
{
    ball.x = SCREEN_WIDTH / 2;
    ball.y = SCREEN_HEIGHT / 2;
    ball.w = 2;
    ball.h = 2;
    ball.dy = 1;
    ball.dx = 1;

    paddle[0].x = 1;
    paddle[0].y = adc1 / 4096 * 110;
    paddle[0].w = 2;
    paddle[0].h = 10;

    paddle[1].x = SCREEN_WIDTH - 1 - 2;
    paddle[1].y = adc2 / 4096 * 110;
    paddle[1].w = 2;
    paddle[1].h = 10;
}

// if return value is 1 collision occured. if return is 0, no collision.
int check_collision(ball_t a, paddle_t b)
{

    int left_a, left_b;
    int right_a, right_b;
    int top_a, top_b;
    int bottom_a, bottom_b;

    left_a = a.x;
    right_a = a.x + a.w;
    top_a = a.y;
    bottom_a = a.y + a.h;

    left_b = b.x;
    right_b = b.x + b.w;
    top_b = b.y;
    bottom_b = b.y + b.h;

    if (left_a > right_b)
    {
        return 0;
    }

    if (right_a < left_b)
    {
        return 0;
    }

    if (top_a > bottom_b)
    {
        return 0;
    }

    if (bottom_a < top_b)
    {
        return 0;
    }

    return 1;
}

/* This routine moves each ball by its motion vector. */
static void move_ball()
{
    // only update ball if timer has looped
    if (timer_1->status == 0b001)
    {
        timer_1->status = 1; // reset status bit

        /* Move the ball by its motion vector. */
        ball.x += ball.dx;
        ball.y += ball.dy;

        // If ball hits the left/right edge of the screen add 1 to score and re-init game;
        if (ball.x < 0)
        {
            score[1] += 1;
            init_game();
        }

        if (ball.x > SCREEN_WIDTH)
        {
            score[0] += 1;
            init_game();
        }

        // If ball hits top/bottom of screen flip y movement vector
        if (ball.y < 0 || ball.y > SCREEN_HEIGHT)
        {
            ball.dy = -ball.dy;
        }

        // check for collision with both paddles
        int i;
        for (i = 0; i < 2; i++)
        {
            int c = check_collision(ball, paddle[i]);

            // collision detected
            if (c == 1)
            {

                // increase ball's speed by 1
                if (ball.dx < 0)
                {
                    ball.dx -= 1;
                }
                else
                {
                    ball.dx += 1;
                }

                // flip ball's direction
                ball.dx = -ball.dx;

                // change ball angle based on where on the paddle it hit
                int hit_pos = (paddle[i].y + paddle[i].h) - ball.y;

                if (hit_pos >= 0 && hit_pos < 2)
                {
                    ball.dy = 2;
                }

                if (hit_pos >= 2 && hit_pos < 5)
                {
                    ball.dy = 1;
                }

                if (hit_pos >= 5 && hit_pos < 7)
                {
                    ball.dy = 0;
                }

                if (hit_pos >= 7 && hit_pos < 10)
                {
                    ball.dy = -1;
                }

                if (hit_pos >= 10 && hit_pos < 12)
                {
                    ball.dy = -2;
                }

                // ball moving right
                if (ball.dx > 0)
                {
                    // teleport ball to avoid mutli collision glitch
                    if (ball.x < 5)
                    {
                        ball.x = 5;
                    }
                }
                else // ball moving left
                {
                    // teleport ball to avoid mutli collision glitch
                    if (ball.x > SCREEN_WIDTH - 5)
                    {
                        ball.x = SCREEN_WIDTH - 5;
                    }
                }
            }
        }
    }
}

static void update_score()
{
    volatile int *hex_ptr = (int *)HEX3_HEX0_BASE;
    volatile int *hex_ptr2 = (int *)HEX5_HEX4_BASE;

    int lookUpTable[10] = {0x3F, 0x6, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x7, 0x7F, 0x67};

    if (score[0] > 99 || score[1] > 99)
    {
        score[0] = 0;
        score[1] = 0;
    }

    // Updating hex display
    *((char *)hex_ptr) = lookUpTable[score[0] % 10];
    *((char *)hex_ptr + 1) = lookUpTable[score[0] / 10];
    *((char *)hex_ptr2) = lookUpTable[score[1] % 10];
    *((char *)hex_ptr2 + 1) = lookUpTable[score[1] / 10];
}

int main(void)
{
    // LCD init
    volatile int delay_count;
    init_spim0();
    init_lcd();
    clear_screen();

    // ADC init
    int mask = Mask_12_bits;
    *(ADC_BASE_ptr) = 0;                 // write anything to channel 0 to update ADC
    adc1 = (*(ADC_BASE_ptr)&mask);       // Read current ADC value (channel 0)
    adc2 = (*(ADC_BASE_ptr + 1) & mask); // Read current ADC value (channel 1)
    printf(adc1 + "\n");
    printf(adc2);

    // Timer init
    // initialize timer for 0.017s interval
    // assumes 100 MHz rate
    timer_1->load = 1700000;

    // start timer for continuous counting
    // 3 is (0b011) for control
    // (1 << 8) is for prescaler
    timer_1->control = 3 + (1 << 8);

    // Game loop
    init_game();
    while (1)
    {
        // UPDATING PADDLE POSITIONS
        *(ADC_BASE_ptr) = 0;                 // write anything to channel 0 to update ADC
        adc1 = (*(ADC_BASE_ptr)&mask);       // Read current ADC value (channel 0)
        adc2 = (*(ADC_BASE_ptr + 1) & mask); // Read current ADC value (channel 1)
        paddle[0].y = adc1 / 4096 * 110;     // Updating paddle y pos
        paddle[1].y = adc2 / 4096 * 110;

        // UPDATEING BALL POSITION
        move_ball();

        // CLEARING SCREEN
        clear_screen();

        // DRAW PADDLES
        LCD_rect(paddle[0].x, paddle[0].y, paddle[0].w, paddle[0].h, 1, 1);
        LCD_rect(paddle[1].x, paddle[1].y, paddle[1].w, paddle[1].h, 1, 1);

        // DRAW BALL
        LCD_rect(ball.x, ball.y, ball.w, ball.h, 1, 1);

        // REFRESH
        refresh_buffer();

        // UPDATE SCORE
        update_score();

        // CHECK FOR RESET
        if ((*btnHardware == 0b0001))
        {
            score[0] = 0;
            score[1] = 0;
            init_game();
        }

        for (delay_count = 5; delay_count != 0; --delay_count)
            ; // delay loop
    }
}