// address_map_arm.h
#define HEX3_HEX0_BASE 0xFF200020
#define HEX5_HEX4_BASE 0xFF200030
#define ADC_BASE 0xFF204000
#define HPS_GPIO1_BASE 0xFF709000
#define HPS_RSTMGR_PREMODRST 0xFFD05014
#define SPIM0_BASE 0xFFF00000
#define SPIM0_SR 0xFFF00028
#define SPIM0_DR 0xFFF00060

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

int main(void)
{
    // LCD init
    init_spim0();
    init_lcd();
    clear_screen();

    // ADC init

    // HEX init

    // Game loop
    while (1)
    {
    }
}