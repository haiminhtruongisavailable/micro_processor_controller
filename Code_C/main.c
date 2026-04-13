#include <16F887.h>

#fuses HS,NOWDT,NOPROTECT,NOLVP
#use delay(clock=20000000)

#use fast_io(B)
#use fast_io(C)
#use fast_io(D)

#byte ANSEL  = 0x188
#byte ANSELH = 0x189

#define PASS_LEN     4
#define MAX_WRONG    3

const char PASSWORD[] = "1234";

#define GREEN_ON     output_high(PIN_B1)
#define GREEN_OFF    output_low(PIN_B1)
#define RED_ON       output_high(PIN_B2)
#define RED_OFF      output_low(PIN_B2)
#define BUZZER_ON    output_high(PIN_C1) 
#define BUZZER_OFF   output_low(PIN_C1)
#define SERVO_HIGH   output_high(PIN_C2) 
#define SERVO_LOW    output_low(PIN_C2)

// ==========================================
// SYSTEM STATES
// ==========================================
typedef enum {
    STATE_IDLE,
    STATE_TYPING,
    STATE_UNLOCKED,
    STATE_ALARM
} SystemState;

SystemState current_state = STATE_IDLE;

volatile int8 idx = 0;
volatile char entered[6]; 
volatile int8 wrong_attempts = 0;
int16 inactivity_timer = 0; 
char last_key = 0;          

// ==========================================
// HARDWARE & AUDIO FUNCTIONS
// ==========================================
void servo_pulse_open()   { SERVO_HIGH; delay_us(2050); SERVO_LOW; delay_ms(18); }
void servo_pulse_closed() { SERVO_HIGH; delay_us(1496); delay_cycles(11); SERVO_LOW; delay_ms(18); }
void success_beep()       { BUZZER_ON; delay_ms(40); BUZZER_OFF; delay_ms(40); BUZZER_ON; delay_ms(40); BUZZER_OFF; }
void error_beep()         { BUZZER_ON; delay_ms(400); BUZZER_OFF; delay_ms(200); BUZZER_ON; delay_ms(400); BUZZER_OFF; }
void ready_beep()         { BUZZER_ON; delay_ms(30); BUZZER_OFF; delay_ms(50); BUZZER_ON; delay_ms(30); BUZZER_OFF; }
void play_key_tick()      { BUZZER_ON; delay_ms(5); BUZZER_OFF; }
void play_delete_tick()   { BUZZER_ON; delay_ms(15); BUZZER_OFF; }
void play_reminder_chirp(){ BUZZER_ON; delay_ms(10); BUZZER_OFF; delay_ms(20); BUZZER_ON; delay_ms(10); BUZZER_OFF; }

char scan_matrix() {
   char k = 0;
   output_high(PIN_D0); delay_us(20);
   if(input(PIN_D4)) k = '7'; else if(input(PIN_D5)) k = '8'; else if(input(PIN_D6)) k = '9'; else if(input(PIN_D7)) k = '/';
   output_low(PIN_D0); if (k) return k;

   output_high(PIN_D1); delay_us(20);
   if(input(PIN_D4)) k = '4'; else if(input(PIN_D5)) k = '5'; else if(input(PIN_D6)) k = '6'; else if(input(PIN_D7)) k = '*';
   output_low(PIN_D1); if (k) return k;

   output_high(PIN_D2); delay_us(20);
   if(input(PIN_D4)) k = '1'; else if(input(PIN_D5)) k = '2'; else if(input(PIN_D6)) k = '3'; else if(input(PIN_D7)) k = '-';
   output_low(PIN_D2); if (k) return k;

   output_high(PIN_D3); delay_us(20);
   if(input(PIN_D4)) k = '%'; else if(input(PIN_D5)) k = '0'; else if(input(PIN_D6)) k = '='; else if(input(PIN_D7)) k = '+';
   output_low(PIN_D3); return k;
}


// ==========================================
// 1. PROCESSING UNIT (The Brains of each State)
// These functions evaluate logic and return the NEXT state.
// ==========================================

SystemState process_idle(char key, int1 key_pressed) {
    if (key_pressed && (key >= '0' && key <= '9')) {
        entered[idx++] = key;   // Save first digit
        play_key_tick();
        return STATE_TYPING;    // Request transition
    }
    return STATE_IDLE;          // Stay in IDLE
}

SystemState process_typing(char key, int1 key_pressed) {
    inactivity_timer += 10; 
    
    if (inactivity_timer >= 5000) { 
        error_beep();
        return STATE_IDLE; 
    }
    
    if (key_pressed) {
        inactivity_timer = 0; 
        
        if (key >= '0' && key <= '9') {
            if (idx < PASS_LEN) {
                entered[idx++] = key;
                play_key_tick();
            } else {
                RED_ON; error_beep(); RED_OFF;
            }
        } 
        else if (key == '-') { 
            if (idx > 0) {
                idx--; entered[idx] = 0; play_delete_tick();
                if (idx == 0) return STATE_IDLE; 
            } else return STATE_IDLE;
        } 
        else if (key == '%') { 
            error_beep();
            return STATE_IDLE;
        } 
        else if (key == '=') { 
            int8 match = 1;
            if (idx == PASS_LEN) {
                for (int8 i=0; i<PASS_LEN; i++) if (entered[i] != PASSWORD[i]) match = 0;
            } else match = 0; 
            
            if (match) return STATE_UNLOCKED;
            
            wrong_attempts++;
            RED_ON; error_beep(); delay_ms(1000); RED_OFF;
            if (wrong_attempts >= MAX_WRONG) return STATE_ALARM;
            else return STATE_IDLE;
        }
    }
    return STATE_TYPING; 
}

SystemState process_unlocked(char key, int1 key_pressed) {
    inactivity_timer += 10; 
    
    if (inactivity_timer == 12000) play_reminder_chirp();
    if (inactivity_timer >= 15000) { ready_beep(); return STATE_IDLE; }
    
    if (key_pressed) {
        if (key == '*') { inactivity_timer = 0; success_beep(); } 
        else if (key == '%') { ready_beep(); return STATE_IDLE; }
        else { error_beep(); }
    }
    return STATE_UNLOCKED;
}

SystemState process_alarm() {
    for(int i = 0; i < 15; i++) { 
        RED_ON; for(int j=0; j<20; j++) { BUZZER_ON; delay_ms(10); BUZZER_OFF; delay_ms(10); } 
        RED_OFF; for(int j=0; j<15; j++) { BUZZER_ON; delay_ms(20); BUZZER_OFF; delay_ms(20); } 
    }
    wrong_attempts = 0; 
    ready_beep();
    return STATE_IDLE; 
}


// ==========================================
// 2. CONTROL UNIT (The State Manager)
// This applies entry-actions whenever a state actually changes.
// ==========================================

void execute_state_transition(SystemState new_state) {
    // 1. Perform Hardware Entry Actions for the new state
    if (new_state == STATE_IDLE) {
        idx = 0;
        for(int i=0; i<6; i++) entered[i] = 0;
        GREEN_OFF;
        for(int j=0; j<10; j++) servo_pulse_closed(); 
    } 
    else if (new_state == STATE_UNLOCKED) {
        wrong_attempts = 0;
        inactivity_timer = 0; 
        GREEN_ON;
        success_beep();
        for(int j=0; j<10; j++) servo_pulse_open();
    }
    else if (new_state == STATE_TYPING) {
        inactivity_timer = 0; 
    }
    
    // 2. Officially update the state
    current_state = new_state;
}


// ==========================================
// EMERGENCY RESET INTERRUPT
// ==========================================
#int_ext
void ext_isr(void) {
   wrong_attempts = 0;
   execute_state_transition(STATE_IDLE);
   GREEN_ON; delay_ms(300); GREEN_OFF;
   ready_beep();
}

// ==========================================
// MAIN LOOP (The Router)
// ==========================================
void main() {
   ANSEL  = 0x00; ANSELH = 0x00;
   set_tris_b(0x01); set_tris_c(0x00); set_tris_d(0xF0); 
   output_b(0x00); output_c(0x00); output_d(0x00); 

   enable_interrupts(INT_EXT);
   ext_int_edge(L_TO_H);
   enable_interrupts(GLOBAL);

   // Boot Sequence
   execute_state_transition(STATE_IDLE);
   ready_beep(); 

   while (TRUE) {
      // Input Phase
      char key = scan_matrix();
      int1 key_pressed = FALSE;
      if (key != 0 && last_key == 0) key_pressed = TRUE; 
      last_key = key;

      SystemState next_state = current_state;

      // Processing Phase (Ask the processing units what to do)
      switch (current_state) {
         case STATE_IDLE:     next_state = process_idle(key, key_pressed); break;
         case STATE_TYPING:   next_state = process_typing(key, key_pressed); break;
         case STATE_UNLOCKED: next_state = process_unlocked(key, key_pressed); break;
         case STATE_ALARM:    next_state = process_alarm(); break;
      }
      
      // Control Phase (If the processing unit requested a new state, execute it!)
      if (next_state != current_state) {
         execute_state_transition(next_state);
      }
      
      // Heartbeat
      delay_ms(10); 
   }
}
