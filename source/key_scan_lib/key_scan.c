/******************************************************************************
*       UART Single-Wire Keyboard Interface Driver Library
*
* Description: Keyboard interface driver function library for BC6xxx and BC759x chips
* Copyright: Beijing Bitcode Technology Co., Ltd. https://bitcode.com.cn
* Version: V1.7
* Initial version: March 10, 2021
* Version history:
*   March 2021   V1.0
*   March 2021   V1.1  Added support for detecting long periods without key activity
*   March 2021   V1.2  Improved execution efficiency
*   March 2021   V1.3  Fixed an error in array definitions
*   March 2021   V1.4  Changed data types from unsigned char to uint8_t, etc.
*   April 2021   V1.5  Added the initial value for LastKeyEvent
*   April 2021   V1.6  Rewrote the format for compatibility with older compiler standards such as C89
*   April 2023   V1.7  Changed the Keyboard initialization code to better support C89 compilers
* Usage:
*   Add the header file key_scan.h to all C source files that need to use the functions in this
*   driver library, and add this file to the project's source file list. Then the functions in this
*   library can be called directly from the user program.
*   Normally, update_key_status() can be called in the UART interrupt, and is_key_changed() can be
*   queried in the main program loop. If the result is nonzero, call get_key_value() to obtain the
*   key value and perform the corresponding keyboard response operation according to that key value.
*   For detailed usage, refer to the UART Single-Wire Keyboard Interface Driver Library Technical Manual.
******************************************************************************/

#include "key_scan.h"

#ifndef NULL
#define NULL 0
#endif

/* Define the keyboard interface data structure and set default values. Here, key release detection is disabled, and the long-press duration is 2000 counts. */
struct keyboard_t
{
    uint8_t         NewKeyAvailable;
    uint8_t         DetectKeyRelease;
    uint8_t         LastKeyEvent;
    uint8_t         ReportedKey;
    uint8_t         CombinedKeyCount;
    uint8_t         LongPressKeyCount;
    uint16_t        LongPressCount;
    uint16_t        TimeCounter;
    const uint8_t** pCombinedKeys;
    const uint8_t** pLongPressKeys;
    uint8_t*        pCombinedKeyStatusArray;
    void (*pCallback)(uint8_t);
};

struct keyboard_t Keyboard = {
    0,          /* NewKeyAvailable */
    0,          /* DetectKeyRelease */
    0xff,       /* LastKeyEvent */
    0,          /* ReportedKey */
    0,          /* CombinedKeyCount */
    0,          /* LongPressKeyCount */
    2000,       /* LongPressCount */
    0,          /* TimeCounter */
    NULL,       /* pCombinedKeys */
    NULL,       /* pLongPressKeys */
    NULL,       /* pCombinedKeyStatusArray */
    NULL        /* pCallback */
};

/* Bit masks corresponding to different numbers of keys. */
const uint8_t FullBits[8] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f };

/******************************************************************************
* Check whether a new key event is available.
* According to the operating mode setting, this function returns whether there is currently an
* unread new key event. Only keyboard changes that match the current operating mode setting will
* be returned. For example, if key release detection is disabled, key releases will not change
* the result of this query.
* Return value: returns 0 if there is no new key event; any other value indicates a new key event.
******************************************************************************/
uint8_t is_key_changed(void)
{
    return Keyboard.NewKeyAvailable;
}

/******************************************************************************
* Set the keyboard detection mode.
* Determines whether to detect key releases, including combined-key releases.
* Parameter: Mode  --  Operating mode: 0 = detect only key presses (closed/conducting),
*                      1 = detect both key presses and key releases.
******************************************************************************/
void set_detect_mode(uint8_t Mode)
{
    Keyboard.DetectKeyRelease = Mode;
}

/******************************************************************************
* Set the long-press count value.
* The long-press duration is determined by this count value. The user should call long_press_tick()
* periodically. Each call increments the internal counter by one, while each keyboard activity resets
* the counter to zero. When the internal counter value is greater than this value, a long-press
* detection is triggered. If the last key event before this, including a combined key, belongs to the
* key values for which long-press detection is required, a long-press key event is reported, using the
* special key value defined by the user to report a new key event.
* Parameter: CountLimit  --  Maximum value 65534. This is the count value required to trigger the
*                            condition. The default value is 2000.
******************************************************************************/
void set_longpress_count(uint16_t CountLimit)
{
    Keyboard.LongPressCount = CountLimit;
}

/******************************************************************************
* Set the callback function.
* In normal applications, the keyboard status update function is called in the UART interrupt, while
* the actual keyboard handler is placed in the main loop. Because processing keyboard events is usually
* not a high-priority operation, handling key events in the main program can reduce the processor time
* occupied by the UART interrupt. When keyboard events need to be handled immediately, or when the
* program is interrupt-driven and has no main loop, this callback function can be set. This function
* will be called automatically after a valid keyboard event is detected.
* This function should have one uint8_t parameter to receive the key value, and its return type should
* be void.
* If it is not set, or is set to NULL, the callback function will not be called. Note that if
* update_key_status() is called from the UART interrupt, the callback function will consume interrupt
* processing time. Interrupts at the same priority level as the UART interrupt, or at a lower priority
* level, will not be executed before this function returns. Therefore, the callback function should not
* take too long to execute.
* Parameter: pCallbackFunc  --  Function pointer to the callback function.
******************************************************************************/
void set_callback(void (*pCallbackFunc)(uint8_t))
{
    Keyboard.pCallback = pCallbackFunc;
}

/******************************************************************************
* Update the keyboard status.
* The input parameter is the data received by the UART. This function is usually called in UART
* interrupt processing, but it can also be called from anywhere as needed.
* Parameter: RxData  --  Data received by the UART from BC6xxx or BC759x.
******************************************************************************/
void update_key_status(uint8_t RxData)
{
    uint8_t        i, j;
    uint8_t        PreviousCBKeyStat;
    const uint8_t* pCurrentKeySet;
    uint8_t        NumOfKeys;

    Keyboard.LastKeyEvent = RxData; /* Save the latest key event. */
    Keyboard.TimeCounter  = 0;
    if (Keyboard.DetectKeyRelease || !(Keyboard.LastKeyEvent & 0x80)) /* If key release detection is enabled, or this is a key press, report a new key event. */
    {
        Keyboard.ReportedKey     = Keyboard.LastKeyEvent;
        Keyboard.NewKeyAvailable = 1;
    }
    if (Keyboard.CombinedKeyCount != 0) /* Combined keys are configured. Check whether the key is part of a combined key. */
    {
        for (i = 0; i < Keyboard.CombinedKeyCount; i++) /* Poll all combined keys. */
        {
            PreviousCBKeyStat = *(Keyboard.pCombinedKeyStatusArray + i);
            pCurrentKeySet    = *(Keyboard.pCombinedKeys + i);
            NumOfKeys         = *(pCurrentKeySet);
            for (j = 0; j < NumOfKeys; j++) /* Poll each key value in the current combined-key definition. */
            {
                if ((Keyboard.LastKeyEvent & 0x7f) == *(pCurrentKeySet + 2 + j)) /* Check whether the key that generated the event is a member of this combined key. */
                {
                    if (Keyboard.LastKeyEvent & 0x80) /* If this is a key release. */
                    {
                        *(Keyboard.pCombinedKeyStatusArray + i) &= ~(1 << j); /* Clear the corresponding flag bit to 0. */
                        if (PreviousCBKeyStat == FullBits[NumOfKeys])         /* If release reporting is enabled and the previous state of the combined key was all keys pressed. */
                        {
                            Keyboard.LastKeyEvent = (*(pCurrentKeySet + 1)) | 0x80; /* The combined-key condition is satisfied. Update the keyboard event to the configured combined-key value. */
                            if (Keyboard.DetectKeyRelease)                          /* If key release detection is enabled. */
                            {
                                Keyboard.ReportedKey     = Keyboard.LastKeyEvent; /* Set the key value to this combined-key release. */
                                Keyboard.NewKeyAvailable = 1;                     /* Set the new-key flag. */
                            }
                        }
                    }
                    else /* If this is a key press. */
                    {
                        *(Keyboard.pCombinedKeyStatusArray + i) |= (1 << j);                /* Set the corresponding flag bit to 1. */
                        if (*(Keyboard.pCombinedKeyStatusArray + i) == FullBits[NumOfKeys]) /* If all keys in the combined key have been pressed. */
                        {
                            Keyboard.LastKeyEvent    = *(pCurrentKeySet + 1); /* The combined-key condition is satisfied. Update the keyboard event to the configured combined-key value. */
                            Keyboard.ReportedKey     = Keyboard.LastKeyEvent; /* Set the key value to this combined-key press. */
                            Keyboard.NewKeyAvailable = 1;                     /* Set the new-key flag. */
                        }
                    }
                    break; /* Exit the current combined-key search and continue checking whether this key also belongs to the next combined-key combination. */
                }
            }
        }
    }
    if (Keyboard.NewKeyAvailable && (Keyboard.pCallback != NULL))
    {
        Keyboard.pCallback(Keyboard.ReportedKey);
        Keyboard.NewKeyAvailable = 0;
    }
}

/******************************************************************************
* Long-press key counting.
* When long-press detection is required, call this function periodically. Each call increments the
* long-press counter by one. The long-press duration is the long-press target count value set by the
* user through set_longpress_count(), multiplied by the time interval between calls to this function.
* This function is generally called in a timer interrupt, but it can also be called from the main
* program loop or from anywhere else as needed.
******************************************************************************/
void long_press_tick(void)
{
    uint8_t i;
    Keyboard.TimeCounter++;
    if (Keyboard.TimeCounter > Keyboard.LongPressCount) /* If the long-press timer has timed out. */
    {
        Keyboard.TimeCounter = 0;
        if (Keyboard.pLongPressKeys != NULL)
        {
            for (i = 0; i < Keyboard.LongPressKeyCount; i++) /* Poll all long-press keys that need to be detected. */
            {
                if ((Keyboard.LastKeyEvent == *(*(Keyboard.pLongPressKeys + i)))
                    || ((Keyboard.LastKeyEvent & 0x80) && (*(*(Keyboard.pLongPressKeys + i)) == 0xFF))) /* If the last key activity is one of the key values that require long-press detection. */
                {
                    Keyboard.ReportedKey     = *(*(Keyboard.pLongPressKeys + i) + 1); /* Set the key value to the user-defined long-press key value. */
                    Keyboard.NewKeyAvailable = 1;
                    if (Keyboard.pCallback != NULL)
                    {
                        Keyboard.pCallback(Keyboard.ReportedKey);
                        Keyboard.NewKeyAvailable = 0;
                    }
                    break;
                }
            }
        }
    }
}

/******************************************************************************
* Get the current key value.
* This function can be called at any time. After this function is called, if the current
* is_key_changed() function returns nonzero, it will return zero after this call. Only key values
* valid under the current operating mode are returned. That is, if key release detection is disabled,
* only key values corresponding to key presses will be returned.
* Return value: Key value. If the current key is a combined key or a long-press key, this function
* returns the user-defined key value for the combined key or long-press key.
******************************************************************************/
uint8_t get_key_value(void)
{
    Keyboard.NewKeyAvailable = 0;
    return Keyboard.ReportedKey;
}

/******************************************************************************
* Define combined keys: def_combined_key().
* This function notifies the driver library of the combined-key definitions and the user-defined
* key values for those combined keys.
* Parameters: pCBKeyList      --  Combined-key list. This is an array whose elements are pointers
*                                 to each combined-key definition array.
*             pCBKeyMap       --  A uint8_t array. The number of array elements must be greater than
*                                 or equal to the number of defined combined keys.
*             CBKeyCount      --  Number of combined keys.
*
* To use the combined-key function, the user must provide the combined-key definitions in the program
* and submit them to the driver library through this function.
* The combined-key definition array has the data type uint8_t. The length of each array is determined
* by the number of keys contained in the combined key. The format is:
* {count, defined_value, key1, key2 ...}. Here, count is the number of keys contained in this combined
* key, defined_value is the user-defined key value representing the combined key, and key1, key2, etc.
* are the original key values of each key.
******************************************************************************/
void def_combined_key(const uint8_t** pCBKeyList, uint8_t* pCBKeyMap, uint8_t CBKeyCount)
{
    uint8_t i;
    Keyboard.pCombinedKeys           = pCBKeyList;
    Keyboard.CombinedKeyCount        = CBKeyCount & 0x07;
    Keyboard.pCombinedKeyStatusArray = pCBKeyMap; /* Keyboard mapping RAM, one byte for each combined key. */
    for (i = 0; i < CBKeyCount; i++)
    {
        *(Keyboard.pCombinedKeyStatusArray + i) = 0; /* Initialize each keyboard status to 0, the released state. */
    }
}

/******************************************************************************
* Define long-press keys: def_longpress_key().
* This function notifies the driver library of the long-press key definitions and the user-defined
* key values corresponding to those long-press keys.
* Parameters: pLPKeyList      --  Long-press key list. This is an array whose elements are pointers
*                                 to long-press key definition arrays.
*             LPKeyCount      --  Number of long-press keys.
*
* The long-press key definition array has the data type uint8_t. Each array consists of two elements,
* in the format:
* {key_value, defined_value}. The first element is the original key value of the key to be detected
* as a long press. This key value may be a native key value from the chip, or a user-defined combined-key
* value. It may also be either the key value corresponding to a key press or the key value corresponding
* to a key release. The second element is the user-defined key value representing this long-press key.
******************************************************************************/
void def_longpress_key(const uint8_t** pLPKeyList, uint8_t LPKeyCount)
{
    Keyboard.pLongPressKeys    = pLPKeyList;
    Keyboard.LongPressKeyCount = LPKeyCount;
}
