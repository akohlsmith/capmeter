/*
 * capmeter.c
 *
 * Created: 11/04/2015 22:34:20
 *  Author: limpkin
 */
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <avr/io.h>
#include <stdio.h>
#include "automated_testing.h"
#include "conversions.h"
#include "measurement.h"
#include "calibration.h"
#include "interrupts.h"
#include "meas_io.h"
#include "serial.h"
#include "utils.h"
#include "vbias.h"
#include "tests.h"
#include "dac.h"
#include "adc.h"
#include "usb.h"
// Define the bootloader function
bootloader_f_ptr_type start_bootloader = (bootloader_f_ptr_type)(BOOT_SECTION_START/2);
// RC Calibration values
#define RCOSC32M_offset  0x03
#define RCOSC32MA_offset 0x04
// USB message
usb_message_t usb_packet;


/*
 * Switch to 32MHz clock
 */
void switch_to_32MHz_clock(void)
{
    DFLLRC32M.CALA = ReadCalibrationByte(PROD_SIGNATURES_START + RCOSC32MA_offset); // Load calibration value for 32M RC Oscillator
    DFLLRC32M.CALB = ReadCalibrationByte(PROD_SIGNATURES_START + RCOSC32M_offset);  // Load calibration value for 32M RC Oscillator
    OSC.CTRL |= OSC_RC32MEN_bm;                                                     // Enable 32MHz oscillator
    while((OSC.STATUS & OSC_RC32MRDY_bm) == 0);                                     // Wait for stable oscillator
    CCP = CCP_IOREG_gc;                                                             // Inform process we change a protected register
    CLK.CTRL = CLK_SCLKSEL_RC32M_gc;                                                // Switch to 32MHz    
    OSC.CTRL &= (~OSC_RC2MEN_bm);                                                   // Disable the default 2Mhz oscillator    
    OSC.XOSCCTRL = OSC_XOSCSEL_32KHz_gc;                                            // Choose 32kHz external crystal
    OSC.CTRL |= OSC_XOSCEN_bm;                                                      // Enable external oscillator
    while((OSC.STATUS & OSC_XOSCRDY_bm) == 0);                                      // Wait for stable 32kHz clock
    OSC.DFLLCTRL = OSC_RC32MCREF_XOSC32K_gc;                                        // Select the 32kHz clock as calibration ref for our 32M
    DFLLRC32M.CTRL = DFLL_ENABLE_bm;                                                // Enable DFLL for RC32M
}

/*
 * Our main
 */
int main(void)
{
    //start_bootloader();
    switch_to_32MHz_clock();                        // Switch to 32MHz
    _delay_ms(1000);                                // Wait for power to settle
    init_serial_port();                             // Initialize serial port    
    init_dac();                                     // Init DAC
    init_adc();                                     // Init ADC
    init_ios();                                     // Init IOs
    init_calibration();                             // Init calibration
    enable_interrupts();                            // Enable interrupts
    init_usb(); 
    //enable_bias_voltage(850);while(1);
    //automated_current_testing();
    //ramp_bias_voltage_test();
    //voltage_settling_test();
    //automated_vbias_testing();
    //automated_current_testing();
    //peak_to_peak_adc_noise_measurement_test();
    //ramp_bias_voltage_test();
    //printf("blah\r\n");_delay_ms(3333);
    //ramp_current_test();
    //functional_test();
    //while(1);
    //calibrate_thresholds();                         // Calibrate vup vlow & thresholds
    //calibrate_cur_mos_0nA();                        // Calibrate 0nA point and store values in eeprom
    //calibrate_current_measurement();                // Calibrate the ADC for current measurements
    
/*
    while(1);
    uint16_t cur_measure = 0;
    uint16_t dac_val = 1200;
    enable_bias_voltage(11500);    
    //update_vbias_dac(--dac_val);while(1);
    set_current_measurement_ampl(CUR_MES_1X);
    while ((cur_measure < get_max_value_for_diff_channel(CUR_MES_1X)) && (dac_val >= VBIAS_MAX_DAC_VAL))
    {
        update_vbias_dac(--dac_val);
        _delay_us(20);
        //cur_measure = get_averaged_stabilized_adc_value(8, 4, TRUE);
        cur_measure = get_averaged_adc_value(11);
        measdprintf("Quiescent current: %u, approx %u/%unA\r\n", cur_measure, compute_cur_mes_numerator_from_adc_val(cur_measure), 1 << get_configured_adc_ampl());
    }
    while(1);*/
    
    // Current mes
//     _delay_ms(1000);
//     enable_bias_voltage(4411);
//     while(1)
//     {
//         for (uint8_t i = 0; i <= CUR_MES_4X; i++)
//         {
//             set_current_measurement_mode(i);
//             print_compute_cur_formula(cur_measurement_loop(17));
//         }        
//     }
//     while(1);

    while(1)
    {
        if (usb_receive_data((uint8_t*)&usb_packet) == TRUE)
        {
            printf("RECEIVED\r\n");
            switch(usb_packet.command_id)
            {
                case CMD_PING: 
                {
                    printf("ping\r\n");
                    // Ping packet, resend the same one
                    usb_send_data((uint8_t*)&usb_packet);
                    break;
                }
                case CMD_VERSION:
                {
                    printf("version\r\n");
                    // Version request packet
                    strcpy((char*)usb_packet.payload, CAPMETER_VER);
                    usb_packet.length = sizeof(CAPMETER_VER);
                    usb_send_data((uint8_t*)&usb_packet);
                    break;
                }
                case CMD_OE_CALIB_STATE:
                {
                    printf("calib state\r\n");
                    // Get open ended calibration state.
                    if (is_platform_calibrated() == TRUE)
                    {
                        // Calibrated, return calibration data
                        usb_packet.length = get_openended_calibration_data(usb_packet.payload);
                    } 
                    else
                    {
                        // Not calibrated, return 0
                        usb_packet.length = 1;
                        usb_packet.payload[0] = 0;
                    }
                    usb_send_data((uint8_t*)&usb_packet);    
                    break;                
                }
                case CMD_OE_CALIB_START:
                {
                    printf("calib start\r\n");
                    // Calibration start
                    start_openended_calibration(usb_packet.payload[0], usb_packet.payload[1], usb_packet.payload[2]);
                    usb_packet.length = get_openended_calibration_data(usb_packet.payload);
                    usb_send_data((uint8_t*)&usb_packet);    
                    break;                
                }
                case CMD_GET_OE_CALIB:
                {
                    printf("calib data\r\n");
                    // Get calibration data
                    usb_packet.length = get_openended_calibration_data(usb_packet.payload);
                    usb_send_data((uint8_t*)&usb_packet);    
                    break;                
                }
                case CMD_SET_VBIAS:
                {
                    uint16_t* temp_vbias = (uint16_t*)usb_packet.payload;
                    uint16_t set_vbias = enable_bias_voltage(*temp_vbias);
                    usb_packet.length = 2;
                    memcpy((void*)usb_packet.payload, (void*)&set_vbias, sizeof(set_vbias));
                    usb_send_data((uint8_t*)&usb_packet);
                    break;
                }
                case CMD_DISABLE_VBIAS:
                {
                    usb_packet.length = 0;
                    disable_bias_voltage();
                    usb_send_data((uint8_t*)&usb_packet);
                    break;
                }
                default: break;
            }
        }
        //_delay_ms(100);
        //printf("-");
    }

    enable_bias_voltage(4500);
    set_capacitance_measurement_mode();
    while(1)
    {
        cap_measurement_loop(FALSE);    
    }
    
    // Freq mes
    uint16_t voltage = 1000;
    uint8_t temp_bool = FALSE;
    enable_bias_voltage(voltage);
    set_capacitance_measurement_mode();
    set_measurement_mode_io(RES_1K);   
    while(1)
    {
        if (cap_measurement_loop(temp_bool) == TRUE)
        {
            if (temp_bool == TRUE)
            {
                temp_bool = FALSE;
            } 
            else
            {
                disable_feedback_mos();
                voltage += 250;
                update_bias_voltage(voltage);
                if (voltage >= 15000)
                {
                    while(1);
                }
                enable_feedback_mos();
                temp_bool = TRUE;
            }              
        } 
    }
}
