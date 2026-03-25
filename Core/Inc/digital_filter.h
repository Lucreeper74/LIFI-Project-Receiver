#ifndef _H_DIGITAL_FILTER_
#define _H_DIGITAL_FILTER_

#include "stm32_opal_receiver.h"
#include <stdint.h>

#define BUF_SIZE     4
#define DC_OFFSET    0 //2048

/*
*   Definition of Digital Filter handle structure
*/
typedef struct {
    OPAL_Receiver_Handle*   hrx;                /* !< Associated OPAL RX Handle */
    uint16_t                y_buf[BUF_SIZE];    /* !< Output sample buffer (volatile for ITR access) */
    size_t                  last_y_index;       /* !< Output buffer index */
    volatile bool           DMA_CPLT_flag;      /* !< Indicates which half of the DMA buffer is filled (Volatile for ITR access) */
} FILTER_Handle;

extern FILTER_Handle hfilter; // Global Digital Filter handle


void FILTER_Init(OPAL_Receiver_Handle* hrx);

uint16_t compute_filter(void);

void FILTER_Buffer_Callback(FILTER_Handle* hfilter, bool is_first_half);

/*
 * Get the index in the circular buffer at the sample n-i
 */
inline static uint16_t get_at_index(uint16_t* buffer, size_t buffer_size, size_t i, size_t offset, uint8_t oversampling) {
        return buffer[(offset + (buffer_size - i * oversampling)) % buffer_size];
}

inline static int get_x_index(size_t i, size_t offset) {
    return get_at_index((uint16_t*) hrx.DMA_ADC_buffer, OPAL_ADC_BUFFER_SIZE, i, offset, OPAL_OVERSAMPLING_FACTOR);
}

inline static int get_y_index(size_t i) {
    return get_at_index(hfilter.y_buf, BUF_SIZE, i, hfilter.last_y_index, 1);
}

#endif // _H_DIGITAL_FILTER_