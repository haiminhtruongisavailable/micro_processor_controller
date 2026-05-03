#include <16F887.h>
#fuses HS,NOWDT,NOPROTECT,NOLVP
#use delay(clock=20000000)
#use fast_io(B)
#use fast_io(C)
#use fast_io(D)

#byte ANSEL  = 0x188
#byte ANSELH = 0x189

#define PASS_LEN  4
#define MAX_WRONG 3
const char PASSWORD[] = "1234";

// ==================== HARDWARE MACROS ====================
#define GREEN_ON     output_high(PIN_B1)
#define GREEN_OFF    output_low(PIN_B1)
#define RED_ON       output_high(PIN_B2)
#define RED_OFF      output_low(PIN_B2)
#define BUZZER_ON    output_high(PIN_C1)
#define BUZZER_OFF   output_low(PIN_C1)
#define SERVO_HIGH   output_high(PIN_C2)
#define SERVO_LOW    output_low(PIN_C2)

// ==================== READABLE ACTIONS ====================
typedef enum {
    ACTION_KEY_TICK,
    ACTION_DELETE_TICK,
    ACTION_SUCCESS_BEEP,
    ACTION_ERROR_BEEP,
    ACTION_READY_BEEP,
    ACTION_REMINDER_CHIRP,
    ACTION_ALARM_CHIRP,
    ACTION_SERVO_OPEN,
    ACTION_SERVO_CLOSE
} ActionType;

void do_action(ActionType act) {
   switch(act) {
      case ACTION_KEY_TICK:      BUZZER_ON; delay_ms(5);  BUZZER_OFF; break;
      case ACTION_DELETE_TICK:   BUZZER_ON; delay_ms(15); BUZZER_OFF; break;
      case ACTION_SUCCESS_BEEP:  BUZZER_ON; delay_ms(40); BUZZER_OFF; delay_ms(40);
                                 BUZZER_ON; delay_ms(40); BUZZER_OFF; break;
      case ACTION_ERROR_BEEP:    BUZZER_ON; delay_ms(400); BUZZER_OFF; delay_ms(200);
                                 BUZZER_ON; delay_ms(400); BUZZER_OFF; break;
      case ACTION_READY_BEEP:    BUZZER_ON; delay_ms(30); BUZZER_OFF; delay_ms(50);
                                 BUZZER_ON; delay_ms(30); BUZZER_OFF; break;
      case ACTION_REMINDER_CHIRP: BUZZER_ON; delay_ms(10); BUZZER_OFF; delay_ms(20);
                                 BUZZER_ON; delay_ms(10); BUZZER_OFF; break;
      case ACTION_ALARM_CHIRP:   for(int j=0; j<6; j++) { BUZZER_ON; delay_ms(80); BUZZER_OFF; delay_ms(40); } break;
      case ACTION_SERVO_OPEN:    SERVO_HIGH; delay_us(2050); SERVO_LOW; delay_ms(18); break;
      case ACTION_SERVO_CLOSE:   SERVO_HIGH; delay_us(1496); delay_cycles(11); SERVO_LOW; delay_ms(18); break;
   }
}

// ==================== FSM STATES ====================
typedef enum {
    STATE_IDLE,
    STATE_TYPING,
    STATE_UNLOCKED,
    STATE_ALARM
} SystemState;
<<<<<<< HEAD

SystemState current_state = STATE_IDLE;

int8  idx = 0;
char  entered[6];
int8  wrong_attempts = 0;
int16 inactivity_timer = 0;
char  last_key = 0;
int1  reminder_sent = 0;
=======

SystemState current_state = STATE_IDLE;

int8  idx = 0;
char  entered[6];
int8  wrong_attempts = 0;
int16 inactivity_timer = 0;
char  last_key = 0;
int1  reminder_sent = 0;

// ==================== HIGH-LEVEL FUNCTIONS ====================
// Logic Layer
int1 password_match() {
   if (idx != PASS_LEN) return 0;
   for (int8 i = 0; i < PASS_LEN; i++) {
      if (entered[i] != PASSWORD[i]) return 0;
   }
   return 1;
}

void update_inactivity_timer() {
   inactivity_timer += 10;
}
void check_typing_timeout() {
   if (inactivity_timer >= 5000) {
      do_action(ACTION_ERROR_BEEP);
      go_to_idle();
   }
}

void check_unlocked_timeout() {
   if (inactivity_timer >= 15000) {
      go_to_idle();
      do_action(ACTION_READY_BEEP);
   }
   else if (inactivity_timer >= 12000 && !reminder_sent) {
      do_action(ACTION_REMINDER_CHIRP);
      reminder_sent = 1;
   }
}
//Application layer
void go_to_idle() {
   current_state = STATE_IDLE;
   idx = 0;
   for(int i=0; i<6; i++) entered[i] = 0;
   GREEN_OFF;
   for(int j=0; j<10; j++) do_action(ACTION_SERVO_CLOSE);
   reminder_sent = 0;
}

void go_to_unlocked() {
   current_state = STATE_UNLOCKED;
   wrong_attempts = 0;
   inactivity_timer = 0;
   reminder_sent = 0;
   GREEN_ON;
   do_action(ACTION_SUCCESS_BEEP);
   for(int j=0; j<10; j++) do_action(ACTION_SERVO_OPEN);
}


void process_key_in_typing(char key) {
   if (key >= '0' && key <= '9') {
      if (idx < PASS_LEN) {
         entered[idx++] = key;
         do_action(ACTION_KEY_TICK);
      } else {
         RED_ON; do_action(ACTION_ERROR_BEEP); RED_OFF;
      }
   }
   else if (key == '-') {
      if (idx > 0) {
         idx--;
         entered[idx] = 0;
         do_action(ACTION_DELETE_TICK);
         if (idx == 0) go_to_idle();
      } else go_to_idle();
   }
   else if (key == '%') {
      do_action(ACTION_ERROR_BEEP);
      go_to_idle();
   }
   else if (key == '=') {
      if (password_match()) {
         go_to_unlocked();
      } else {
         wrong_attempts++;
         RED_ON; do_action(ACTION_ERROR_BEEP); delay_ms(1000); RED_OFF;
         if (wrong_attempts >= MAX_WRONG) {
            current_state = STATE_ALARM;
         } else {
            go_to_idle();
         }
      }
   }
}

void process_key_in_unlocked(char key) {
   if (key == '*') {
      inactivity_timer = 0;
      do_action(ACTION_SUCCESS_BEEP);
   }
   else if (key == '%') {
      go_to_idle();
      do_action(ACTION_READY_BEEP);
   }
   else do_action(ACTION_ERROR_BEEP);
}

// ==================== KEYPAD SCANNER ====================
char scan_matrix() {
   char k = 0;
   output_high(PIN_D0); delay_us(20);
   if(input(PIN_D4)) k = '7'; else if(input(PIN_D5)) k = '8'; else if(input(PIN_D6)) k = '9'; else if(input(PIN_D7)) k = '/';
   output_low(PIN_D0); if(k) return k;

   output_high(PIN_D1); delay_us(20);
   if(input(PIN_D4)) k = '4'; else if(input(PIN_D5)) k = '5'; else if(input(PIN_D6)) k = '6'; else if(input(PIN_D7)) k = '*';
   output_low(PIN_D1); if(k) return k;

   output_high(PIN_D2); delay_us(20);
   if(input(PIN_D4)) k = '1'; else if(input(PIN_D5)) k = '2'; else if(input(PIN_D6)) k = '3'; else if(input(PIN_D7)) k = '-';
   output_low(PIN_D2); if(k) return k;
>>>>>>> 64613931c767224242ae705700b448ea3fb923d3

void go_to_idle() {
   current_state = STATE_IDLE;
   idx = 0;
   for(int i=0; i<6; i++) entered[i] = 0;
   GREEN_OFF;
   for(int j=0; j<10; j++) do_action(ACTION_SERVO_CLOSE);
   reminder_sent = 0;
}

<<<<<<< HEAD
// ==================== HIGH-LEVEL FUNCTIONS ====================
// Logic Layer
int1 password_match() {
   if (idx != PASS_LEN) return 0;
   for (int8 i = 0; i < PASS_LEN; i++) {
      if (entered[i] != PASSWORD[i]) return 0;
   }
   return 1;
}

void update_inactivity_timer() {
   inactivity_timer += 10;
}
void check_typing_timeout() {
   if (inactivity_timer >= 5000) {
      do_action(ACTION_ERROR_BEEP);
      go_to_idle();
   }
}

void check_unlocked_timeout() {
   if (inactivity_timer >= 15000) {
      go_to_idle();
      do_action(ACTION_READY_BEEP);
   }
   else if (inactivity_timer >= 12000 && !reminder_sent) {
      do_action(ACTION_REMINDER_CHIRP);
      reminder_sent = 1;
   }
}
//Application layer


void go_to_unlocked() {
   current_state = STATE_UNLOCKED;
   wrong_attempts = 0;
   inactivity_timer = 0;
   reminder_sent = 0;
   GREEN_ON;
   do_action(ACTION_SUCCESS_BEEP);
   for(int j=0; j<10; j++) do_action(ACTION_SERVO_OPEN);
}


void process_key_in_typing(char key) {
   if (key >= '0' && key <= '9') {
      if (idx < PASS_LEN) {
         entered[idx++] = key;
         do_action(ACTION_KEY_TICK);
      } else {
         RED_ON; do_action(ACTION_ERROR_BEEP); RED_OFF;
      }
   }
   else if (key == '*') {
      // Use '*' as Backspace/Cancel
      if (idx > 0) {
         idx--;
         entered[idx] = 0;
         do_action(ACTION_DELETE_TICK);
         if (idx == 0) go_to_idle();
      } else {
         go_to_idle();
         do_action(ACTION_ERROR_BEEP);
      }
   }
   else if (key == '#') {
      // Use '#' as Enter/Submit
      if (password_match()) {
         go_to_unlocked();
      } else {
         wrong_attempts++;
         RED_ON; do_action(ACTION_ERROR_BEEP); delay_ms(1000); RED_OFF;
         if (wrong_attempts >= MAX_WRONG) {
            current_state = STATE_ALARM;
         } else {
            go_to_idle();
         }
      }
   }
}

void process_key_in_unlocked(char key) {
   if (key == '#') {
      // Keep unlocked timer reset
      inactivity_timer = 0;
      do_action(ACTION_SUCCESS_BEEP);
   }
   else if (key == '*') {
      // Manual Lock
      go_to_idle();
      do_action(ACTION_READY_BEEP);
   }
   else do_action(ACTION_ERROR_BEEP);
}

// ==================== KEYPAD SCANNER (UPDATED for 3x4) ====================
char scan_matrix() {
   char k = 0;
   
   // Row 1 (RD0)
   output_high(PIN_D0); delay_us(20);
   if(input(PIN_D4)) k = '1'; else if(input(PIN_D5)) k = '2'; else if(input(PIN_D6)) k = '3';
   output_low(PIN_D0); if(k) return k;

   // Row 2 (RD1)
   output_high(PIN_D1); delay_us(20);
   if(input(PIN_D4)) k = '4'; else if(input(PIN_D5)) k = '5'; else if(input(PIN_D6)) k = '6';
   output_low(PIN_D1); if(k) return k;

   // Row 3 (RD2)
   output_high(PIN_D2); delay_us(20);
   if(input(PIN_D4)) k = '7'; else if(input(PIN_D5)) k = '8'; else if(input(PIN_D6)) k = '9';
   output_low(PIN_D2); if(k) return k;

   // Row 4 (RD3)
   output_high(PIN_D3); delay_us(20);
   if(input(PIN_D4)) k = '*'; else if(input(PIN_D5)) k = '0'; else if(input(PIN_D6)) k = '#';
   output_low(PIN_D3);
   
   return k;
}

=======
>>>>>>> 64613931c767224242ae705700b448ea3fb923d3
// ==================== EMERGENCY RESET ====================
#int_ext
void ext_isr(void) {
   wrong_attempts = 0;
   go_to_idle();
   GREEN_ON; delay_ms(300); GREEN_OFF;
   do_action(ACTION_READY_BEEP);
}

// ==================== MAIN ====================
void main() {
   ANSEL = 0x00; ANSELH = 0x00;
<<<<<<< HEAD
   
   set_tris_b(0x01); 
   set_tris_c(0x00); 
   // TRISD: RD4, RD5, RD6 as inputs (columns), RD0-RD3 as outputs (rows)
   set_tris_d(0x70); 
   
=======
   set_tris_b(0x01); set_tris_c(0x00); set_tris_d(0xF0);
>>>>>>> 64613931c767224242ae705700b448ea3fb923d3
   output_b(0x00); output_c(0x00); output_d(0x00);

   enable_interrupts(INT_EXT);
   ext_int_edge(L_TO_H);
   enable_interrupts(GLOBAL);

   go_to_idle();
   do_action(ACTION_READY_BEEP);

   while (TRUE) {
      char key = scan_matrix();
      int1 key_pressed = (key != 0 && last_key == 0);
      last_key = key;

      switch (current_state) {
         case STATE_IDLE:
            if (key_pressed && (key >= '0' && key <= '9')) {
               current_state = STATE_TYPING;
               inactivity_timer = 0;
               entered[idx++] = key;
               do_action(ACTION_KEY_TICK);
            }
            break;

         case STATE_TYPING:
            update_inactivity_timer();
            check_typing_timeout();
            if (key_pressed) {
               inactivity_timer = 0;
               process_key_in_typing(key);
            }
            break;

         case STATE_UNLOCKED:
            update_inactivity_timer();
            check_unlocked_timeout();
            if (key_pressed) process_key_in_unlocked(key);
            break;

         case STATE_ALARM:
            for(int i = 0; i < 15; i++) {
               RED_ON;  do_action(ACTION_ALARM_CHIRP);
               RED_OFF; do_action(ACTION_ALARM_CHIRP);
            }
            wrong_attempts = 0;
            go_to_idle();
            break;

         default:
            go_to_idle();
            break;
      }
      delay_ms(10);
   }
}
