/*
 * FreeRTOS Kernel V10.3.1
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * ARMCLANG (Keil Compiler 6) compatible version
 * Originally from portable/RVDS/ARM_CM3, adapted for ARMCLANG
 */

#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Type definitions. */
#define portCHAR        char
#define portFLOAT       float
#define portDOUBLE      double
#define portLONG        long
#define portSHORT       short
#define portSTACK_TYPE  uint32_t
#define portBASE_TYPE   long

typedef portSTACK_TYPE StackType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;

#if( configUSE_16_BIT_TICKS == 1 )
    typedef uint16_t TickType_t;
    #define portMAX_DELAY ( TickType_t ) 0xffff
#else
    typedef uint32_t TickType_t;
    #define portMAX_DELAY ( TickType_t ) 0xffffffffUL
    #define portTICK_TYPE_IS_ATOMIC 1
#endif

/* Architecture specifics. */
#define portSTACK_GROWTH            ( -1 )
#define portTICK_PERIOD_MS          ( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT          8

#define portSY_FULL_READ_WRITE      ( 15 )

/* Scheduler utilities. */
#define portNVIC_INT_CTRL_REG       ( * ( ( volatile uint32_t * ) 0xe000ed04 ) )
#define portNVIC_PENDSVSET_BIT      ( 1UL << 28UL )
#define portEND_SWITCHING_ISR( xSwitchRequired ) if( xSwitchRequired != pdFALSE ) portYIELD()
#define portYIELD_FROM_ISR( x )     portEND_SWITCHING_ISR( x )

#define portYIELD()                                     \
{                                                       \
    portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;     \
    __asm volatile( "dsb 15" ::: "memory" );            \
    __asm volatile( "isb 15" ::: "memory" );            \
}

/* Critical section management. */
extern void vPortEnterCritical( void );
extern void vPortExitCritical( void );

#define portDISABLE_INTERRUPTS()                vPortRaiseBASEPRI()
#define portENABLE_INTERRUPTS()                 vPortSetBASEPRI( 0 )
#define portENTER_CRITICAL()                    vPortEnterCritical()
#define portEXIT_CRITICAL()                     vPortExitCritical()
#define portSET_INTERRUPT_MASK_FROM_ISR()       ulPortRaiseBASEPRI()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x)    vPortSetBASEPRI(x)

/* Tickless idle/low power functionality. */
#ifndef portSUPPRESS_TICKS_AND_SLEEP
    extern void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime );
    #define portSUPPRESS_TICKS_AND_SLEEP( xExpectedIdleTime ) vPortSuppressTicksAndSleep( xExpectedIdleTime )
#endif

/* Port specific optimisations. */
#ifndef configUSE_PORT_OPTIMISED_TASK_SELECTION
    #define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#endif

#if configUSE_PORT_OPTIMISED_TASK_SELECTION == 1
    #if( configMAX_PRIORITIES > 32 )
        #error configUSE_PORT_OPTIMISED_TASK_SELECTION can only be set to 1 when configMAX_PRIORITIES is less than or equal to 32.
    #endif

    #define portRECORD_READY_PRIORITY( uxPriority, uxReadyPriorities ) ( uxReadyPriorities ) |= ( 1UL << ( uxPriority ) )
    #define portRESET_READY_PRIORITY( uxPriority, uxReadyPriorities ) ( uxReadyPriorities ) &= ~( 1UL << ( uxPriority ) )
    #define portGET_HIGHEST_PRIORITY( uxTopPriority, uxReadyPriorities ) uxTopPriority = ( 31UL - ( uint32_t ) __builtin_clz( ( uxReadyPriorities ) ) )
#endif

#define portTASK_FUNCTION_PROTO( vFunction, pvParameters ) void vFunction( void *pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters ) void vFunction( void *pvParameters )

#ifdef configASSERT
    void vPortValidateInterruptPriority( void );
    #define portASSERT_IF_INTERRUPT_PRIORITY_INVALID()  vPortValidateInterruptPriority()
#endif

#define portNOP()
#define portINLINE inline

#ifndef portFORCE_INLINE
    #define portFORCE_INLINE __attribute__((always_inline)) inline
#endif

/* ARMCLANG-compatible inline assembly functions */

static portFORCE_INLINE void vPortSetBASEPRI( uint32_t ulBASEPRI )
{
    __asm volatile( "msr basepri, %0" : : "r" ( ulBASEPRI ) );
}

static portFORCE_INLINE void vPortRaiseBASEPRI( void )
{
uint32_t ulNewBASEPRI = configMAX_SYSCALL_INTERRUPT_PRIORITY;
    __asm volatile (
        "msr basepri, %0   \n"
        "dsb               \n"
        "isb               \n"
        : : "r" ( ulNewBASEPRI ) : "memory"
    );
}

static portFORCE_INLINE void vPortClearBASEPRIFromISR( void )
{
    uint32_t ulZero = 0;
    __asm volatile( "msr basepri, %0" : : "r" ( ulZero ) );
}

static portFORCE_INLINE uint32_t ulPortRaiseBASEPRI( void )
{
uint32_t ulReturn, ulNewBASEPRI = configMAX_SYSCALL_INTERRUPT_PRIORITY;
    __asm volatile (
        "mrs %0, basepri    \n"
        "msr basepri, %1    \n"
        "dsb                \n"
        "isb                \n"
        : "=r" ( ulReturn ) : "r" ( ulNewBASEPRI ) : "memory"
    );
    return ulReturn;
}

static portFORCE_INLINE BaseType_t xPortIsInsideInterrupt( void )
{
uint32_t ulCurrentInterrupt;
    __asm volatile( "mrs %0, ipsr" : "=r" ( ulCurrentInterrupt ) );
    return ( ulCurrentInterrupt == 0 ) ? pdFALSE : pdTRUE;
}

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */
