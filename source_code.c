#include <xc.h>
#pragma config FOSC = HS, WDTE = OFF, PWRTE = ON, BOREN = ON, LVP = OFF

#define _XTAL_FREQ 20000000

#define SERVO_PIN       RC2

#define THRESHOLD       60      // Difference to trigger tracking
#define NEUTRAL_PULSE   1450    // Center position (90°)
#define SERVO_MIN_PULSE  800    // 0° limit
#define SERVO_MAX_PULSE 2200    // 180° limit
#define STEP            10      // Pulse change per loop iteration
#define NEUTRAL_STEP     10

volatile unsigned int servo_pulse = NEUTRAL_PULSE;
volatile unsigned char state = 0;

// ===================== ADC =====================
void ADC_Init() {
    ADCON1 = 0x80;
    ADCON0 = 0x41;
}

unsigned int ADC_Read(unsigned char channel) {
    ADCON0 &= 0xC7;
    ADCON0 |= (channel << 3);
    __delay_us(30);
    GO_nDONE = 1;
    while (GO_nDONE);
    return ((unsigned int)ADRESH << 8) | ADRESL;
}

// ===================== TIMER1 ISR (Servo PWM) =====================
void Timer1_Init() {
    T1CON = 0x31;   // 1:8 prescaler, Timer1 ON
    TMR1IE = 1;
    PEIE   = 1;
    GIE    = 1;
}

void __interrupt() ISR() {
    if (TMR1IF) {
        TMR1IF = 0;
        if (state == 0) {
            SERVO_PIN = 1;
            unsigned int ticks = servo_pulse / 1.6;
            TMR1 = 65536 - ticks;
            state = 1;
        } else {
            SERVO_PIN = 0;
            unsigned int ticks = (20000 - servo_pulse) / 1.6;
            TMR1 = 65536 - ticks;
            state = 0;
        }
    }
}

// ===================== MAIN =====================
void main() {
    TRISA   = 0x03;
    TRISC2  = 0;
    SERVO_PIN  = 0;

    ADC_Init();
    Timer1_Init();

    while (1) {
        unsigned int east = ADC_Read(0);
        unsigned int west = ADC_Read(1);

        int diff = (int)east - (int)west;

        // ---- STATE 1: East is brighter ? track east ----
        if (diff > THRESHOLD) {
            if (servo_pulse > SERVO_MIN_PULSE)
                servo_pulse -= STEP;
        }

        // ---- STATE 2: West is brighter ? track west ----
        else if (diff < -THRESHOLD) {
            if (servo_pulse < SERVO_MAX_PULSE)
                servo_pulse += STEP;
        }

        // ---- STATE 3: Light is balanced ? return to neutral ----
        else {
            // Gradually ease back to center instead of snapping
            if (servo_pulse < NEUTRAL_PULSE - NEUTRAL_STEP) {
                servo_pulse += NEUTRAL_STEP;
            }
            else if (servo_pulse > NEUTRAL_PULSE + NEUTRAL_STEP) {
                servo_pulse -= NEUTRAL_STEP;
            }
            else {
                servo_pulse = NEUTRAL_PULSE;  // Lock to exact center
            }
        }

        __delay_ms(20);
    }
}