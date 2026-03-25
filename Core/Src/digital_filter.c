#include "digital_filter.h"

// Low Pass with Fe=25 Hz, m=1, Fc=5 Hz
float b_coef[BUF_SIZE] = { 0.1489, 0.2978, 0.1489,  0 };
float a_coef[BUF_SIZE] = { 1,       0.4565, -0.0521, 0 };

FILTER_Handle hfilter;

void FILTER_Init(OPAL_Receiver_Handle* hrx) {
    hfilter.hrx             = hrx;
    hfilter.last_y_index    = 0;
    hfilter.DMA_CPLT_flag   = true;
}

uint16_t compute_filter(void) {
    size_t x_offset = hfilter.DMA_CPLT_flag ? (OPAL_ADC_BUFFER_SIZE-1) : (OPAL_ADC_BUFFER_HALF_SIZE-1);

    uint16_t result = b_coef[0] * (get_x_index(0, x_offset) - DC_OFFSET) // b0 * x(n)
                    + b_coef[1] * (get_x_index(1, x_offset) - DC_OFFSET) // b1 * x(n-1)
                    + b_coef[2] * (get_x_index(2, x_offset) - DC_OFFSET) // b2 * x(n-2)
 
                    + a_coef[1] * get_y_index(1)                       // a1 * y(n-1)
                    + a_coef[2] * get_y_index(2);                      // a2 * y(n-2)

    hfilter.y_buf[hfilter.last_y_index++] = result;
    if (hfilter.last_y_index >= BUF_SIZE)
        hfilter.last_y_index = 0;

    result = result >= 0 ? result : 0;
    return (result + DC_OFFSET);
}

void FILTER_Buffer_Callback(FILTER_Handle* hfilter, bool is_first_half) {
    hfilter->DMA_CPLT_flag = !is_first_half;
}