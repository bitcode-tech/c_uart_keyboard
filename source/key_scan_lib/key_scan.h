#ifndef KEY_SCAN_H
#define KEY_SCAN_H

/******************************************************************************
*       UART Single-Wire Keyboard Interface Driver Library
*
* Program Description: Driver function library for the keyboard interfaces of
* BC6xxx and BC759x chips.
* Copyright: Beijing Bitcode Technology Co., Ltd. https://bitcode.com.cn
* Version: V1.7
* Initial Version: March 10, 2021
* Version History:
*   March 2021   V1.0
*   March 2021   V1.1  Added support for detecting long periods with no key activity
*   March 2021   V1.2  Improved execution efficiency
*   March 2021   V1.3  Fixed an error in array definitions
*   March 2021   V1.4  Changed data types from unsigned char to uint8_t, etc.
*   April 2021   V1.5  Added the initial value for LastKeyEvent
*   April 2021   V1.6  Rewritten in a format compatible with older compiler standards such as C89
*   April 2023   V1.7  Changed the Keyboard initialization code to better support C89 compilers
* Usage:
*   Add this header file to all C source files that need to use functions from this
*   driver library, and add key_scan.c to the project source file list. The functions
*   in this library can then be called directly from the user program.
*   Usually, update_key_status() can be called from the UART interrupt, and
*   is_key_changed() can then be queried from the main program loop. If the result
*   is non-zero, call get_key_value() to obtain the key value and perform the
*   corresponding keyboard response operation.
*   For detailed usage, refer to the "UART Single-Wire Keyboard Interface Driver
*   Library Technical Specification".
******************************************************************************/

#include <stdint.h>
/*
stdint.h contains the definitions of data types such as uint8_t. If your
environment uses a standard earlier than C99 or does not provide stdint.h,
define them yourself as follows:
typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
*/

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
* Query whether a new key is available: is_key_changed()
* According to the working mode setting, returns whether there is currently an
* unread new key event. Only keyboard changes that match the current working mode
* setting are reported. For example, if key release detection is disabled, key
* release events will not change the result of this query.
* Return value: returns 0 if there is no new key; any other value indicates that
* a new key is available.
******************************************************************************/
uint8_t is_key_changed(void);

/******************************************************************************
* Set the keyboard detection mode: set_detect_mode()
* Determines whether to detect the release of keys, including combination keys.
* Parameter: Mode  --  working mode, 0 = detect only key press (conduction),
*                      1 = detect both key press and key release
******************************************************************************/
void set_detect_mode(uint8_t Mode);

/******************************************************************************
* Set the long-press count value: set_longpress_count()
* The long-press duration is determined by this count value. The user periodically
* calls long_press_tick(). Each call increments the internal counter by one, while
* each keyboard activity clears the counter. When the internal counter value is
* greater than this value, a long-press detection is triggered. If the last key
* event before this, including a combination key, belongs to a key value that
* needs long-press detection, a long-press key is reported. This is reported as a
* new key using the special key value set by the user.
* Parameter: CountLimit  --  maximum value 65534; the count value required to
*                            trigger the condition. The default value is 2000.
******************************************************************************/
void set_longpress_count(uint16_t CountLimit);

/******************************************************************************
* Set the callback function: set_callback()
* In normal applications, the keyboard status update function is called from the
* UART interrupt, while the actual keyboard handling routine is placed in the main
* loop. Because handling keyboard events is usually not a high-priority operation,
* processing keys in the main program can reduce the processor time occupied by
* the UART interrupt. If keyboard events need to be handled immediately, or if the
* program is interrupt-driven and has no main loop, this callback function can be
* set. It will be called automatically after a valid keyboard event is detected.
* The callback function should have one uint8_t input parameter to receive the key
* value, and its return type should be void.
* If it is not set or is set to NULL, the callback function will not be called.
* Please note that if update_key_status() is called from the UART interrupt, this
* callback function will consume interrupt processing time. Interrupts at the same
* priority level as the UART interrupt, or at a lower priority level, will not be
* executed before it returns. Therefore, the callback function should not take too
* much time to execute.
* Parameter: pCallbackFunc  --  function pointer to the callback function
******************************************************************************/
void set_callback(void (*pCallbackFunc)(uint8_t));

/******************************************************************************
* Update keyboard status: update_key_status()
* The input parameter is the data received by the UART. This function is usually
* called in UART interrupt handling, but it can also be called from anywhere as
* needed.
* Parameter: RxData  --  data received by the UART from BC6xxx or BC759x
******************************************************************************/
void update_key_status(uint8_t RxData);

/******************************************************************************
* Long-press key counter: long_press_tick()
* When long-press detection is required, call this function periodically. Each
* call increments the long-press counter by one. The long-press time is equal to
* the long-press target count value set by the user through set_longpress_count()
* multiplied by the interval at which this function is called. This function is
* usually called from a timer interrupt, but it can also be called from the main
* program loop or any other place as needed.
******************************************************************************/
void long_press_tick(void);

/******************************************************************************
* Get the current key value: get_key_value()
* This function can be called at any time. After this function is called, if the
* current is_key_changed() function returns non-zero, the return value will become
* zero after the call. It only returns key values that are valid under the current
* working mode. That is, if key release detection is disabled, only the key values
* corresponding to key presses will be returned.
* Return value: key value. If the current key is a combination key or a long-press
* key, the user-defined key value for that combination key or long-press key is
* returned.
******************************************************************************/
uint8_t get_key_value(void);

/******************************************************************************
* Define combination keys: def_combined_key()
* This function informs the driver library of the combination key definitions and
* their user-defined key values.
* Parameters: pCBKeyList      --  combination key list. This is an array whose
*                                elements are pointers to the definition arrays
*                                for each combination key.
*             pCBKeyMap       --  a uint8_t array. The number of elements in this
*                                array must be greater than or equal to the
*                                number of defined combination keys.
*             CBKeyCount      --  number of combination keys
*
* To use the combination key function, the user must provide the combination key
* definitions in the program and submit them to the driver library through this
* function.
* The combination key definition array has the data type uint8_t. The length of
* each array is determined by the number of keys in the combination. The format is
* {count, defined_value, key1, key2 ... }. Here, count is the number of keys
* included in this combination key; defined_value is the user-defined key value
* that represents the combination key; and key1, key2, etc. are the original key
* values of each key.
******************************************************************************/
void def_combined_key(const uint8_t** pCBKeyList, uint8_t* pCBKeyMap, uint8_t CBKeyCount);

/******************************************************************************
* Define long-press keys: def_longpress_key()
* This function informs the driver library of the long-press key definitions and
* the corresponding long-press key values set by the user.
* Parameters: pLPKeyList      --  long-press key list. This is an array whose
*                                elements are pointers to the long-press key
*                                definition arrays.
*             LPKeyCount      --  number of long-press keys
*
* The long-press key definition array has the data type uint8_t. Each array
* consists of two elements, in the format {key_value, defined_value}. The first
* element is the original key value for which long-press detection is required.
* This key value can be a native chip key value or a user-defined combination key
* value, and it can correspond to either a key press value or a key release value.
* The second parameter is the user-defined key value that represents this
* long-press key.
******************************************************************************/
void def_longpress_key(const uint8_t** pLPKeyList, uint8_t LPKeyCount);

#ifdef __cplusplus
}
#endif

#endif
