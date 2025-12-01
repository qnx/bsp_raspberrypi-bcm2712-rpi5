/*
 * Copyright (c) 2024, BlackBerry Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You
 * may not reproduce, modify or distribute this software except in
 * compliance with the License. You may obtain a copy of the License
 * at: http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 *
 * This file may contain contributions from others, either as
 * contributors under the License or as licensors under other terms.
 * Please review this entire file for other proprietary rights or license
 * notices, as well as the QNX Development Suite License Guide at
 * http://licensing.qnx.com/license-guide/ for other information.
 * $
 */


#include <dwc_variant.h>


/**
 *  @brief             SPI driver cleanup function.
 *  @param  hdl        SPI driver handler.
 *
 *  @return            Void.
 */
void dw_fini(void *const hdl)
{
    dw_spi_t  *const spi = hdl;
    dw_spi_enable(spi, DW_SPI_DISABLE);

#ifdef DW_DMA
    /* Clean up DMA */
    dw_spi_fini_dma(spi);
#endif

    /* Reset interrupt status */
    dw_read32(spi, DW_SPI_TXOICR);
    dw_read32(spi, DW_SPI_RXOICR);
    dw_read32(spi, DW_SPI_RXUICR);

    dw_spi_deselect_slave(spi);

    /* Detach the interrupt */
    if (spi->iid != -1) {
        InterruptDetach(spi->iid);
    }

    /* Unmap SPI register */
    if (spi->vbase != (uintptr_t)MAP_FAILED) {
        munmap_device_memory((void *)spi->vbase, spi->map_size);
    }

    /* Free SPI structure */
    free(spi);
}
