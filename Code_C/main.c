#include <16F887.h>
#fuses HS, NOWDT, NOPROTECT, NOLVP
#use delay(clock=20000000)

// C?u hình I/O nhanh cho các Port
#use fast_io(B)
#use fast_io(C)
#use fast_io(D)

#byte ANSEL  = 0x188
#byte ANSELH = 0x189

// ==================== C?U HÌNH H? TH?NG ====================
#define PASS_LEN  4
#define MAX_WRONG 3
const char PASSWORD[] = "1234";

// Ð?nh nghia chân ph?n c?ng
#define GREEN_LED    PIN_B1
#define RED_LED      PIN_B2
#define BUZZER       PIN_C1
#define SERVO_PIN    PIN_C2

// ==================== Ð?NH NGHIA HÀNH Ð?NG ====================
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
      case ACTION_KEY_TICK:      output_high(BUZZER); delay_ms(5);  output_low(BUZZER); break;
      case ACTION_DELETE_TICK:   output_high(BUZZER); delay_ms(15); output_low(BUZZER); break;
      case ACTION_SUCCESS_BEEP:  output_high(BUZZER); delay_ms(40); output_low(BUZZER); delay_ms(40);
                                 output_high(BUZZER); delay_ms(40); output_low(BUZZER); break;
      case ACTION_ERROR_BEEP:    output_high(BUZZER); delay_ms(400); output_low(BUZZER); delay_ms(200);
                                 output_high(BUZZER); delay_ms(400); output_low(BUZZER); break;
      case ACTION_READY_BEEP:    output_high(BUZZER); delay_ms(30); output_low(BUZZER); delay_ms(50);
                                 output_high(BUZZER); delay_ms(30); output_low(BUZZER); break;
      case ACTION_REMINDER_CHIRP: output_high(BUZZER); delay_ms(10); output_low(BUZZER); delay_ms(20);
                                 output_high(BUZZER); delay_ms(10); output_low(BUZZER); break;
      case ACTION_ALARM_CHIRP:   for(int j=0; j<6; j++) { output_high(BUZZER); delay_ms(80); output_low(BUZZER); delay_ms(40); } break;
      case ACTION_SERVO_OPEN:    output_high(SERVO_PIN); delay_us(2050); output_low(SERVO_PIN); delay_ms(18); break;
      case ACTION_SERVO_CLOSE:   output_high(SERVO_PIN); delay_us(1499); output_low(SERVO_PIN); delay_ms(18); break;
   }
}


typedef enum {
    STATE_IDLE,
    STATE_TYPING,
    STATE_UNLOCKED,
    STATE_ALARM
} SystemState;

SystemState current_state = STATE_IDLE;
int8  idx = 0;
char  entered[6];
int8  wrong_attempts = 0;
int16 inactivity_timer = 0;
char  last_key = 0;
int1  reminder_sent = 0;

void go_to_idle() {
    current_state = STATE_IDLE;
    idx = 0;
    for(int i=0; i<6; i++) entered[i] = 0;
    output_low(GREEN_LED);
    output_low(RED_LED);
    do_action(ACTION_SERVO_CLOSE);
    reminder_sent = 0;
}

void go_to_unlocked() {
    current_state = STATE_UNLOCKED;
    wrong_attempts = 0;
    inactivity_timer = 0;
    reminder_sent = 0;
    output_high(GREEN_LED);
    do_action(ACTION_SUCCESS_BEEP);
    do_action(ACTION_SERVO_OPEN);
}

// ==================== LOGIC X? LÝ ====================
int1 password_match() {
    if (idx != PASS_LEN) return 0;
    for (int8 i = 0; i < PASS_LEN; i++) {
        if (entered[i] != PASSWORD[i]) return 0;
    }
    return 1;
}

void process_key_in_typing(char key) {
    if (key >= '0' && key <= '9') {
        if (idx < PASS_LEN) {
            entered[idx++] = key;
            do_action(ACTION_KEY_TICK);
        } else {
            output_high(RED_LED); do_action(ACTION_ERROR_BEEP); output_low(RED_LED);
        }
    }
    else if (key == '*') { // Backspace/Cancel
        if (idx > 0) {
            idx--;
            entered[idx] = 0;
            do_action(ACTION_DELETE_TICK);
            if (idx == 0) { 
            go_to_idle();
            do_action(ACTION_ERROR_BEEP);}
        } else {
            go_to_idle();
            do_action(ACTION_ERROR_BEEP);
        }
    }
    else if (key == '#') { // Submit
        if (password_match()) {
            go_to_unlocked();
        } else {
            wrong_attempts++;
            output_high(RED_LED); do_action(ACTION_ERROR_BEEP); delay_ms(1000); output_low(RED_LED);
            if (wrong_attempts >= MAX_WRONG) current_state = STATE_ALARM;
            else go_to_idle();
        }
    }
}

// ==================== QUÉT BÀN PHÍM (PORT D) ====================
char scan_matrix() {
    char k = 0;
    // Row 1
    output_high(PIN_D0); delay_us(20);
    if(input(PIN_D4)) k = '1'; else if(input(PIN_D5)) k = '2'; else if(input(PIN_D6)) k = '3';
    output_low(PIN_D0); if(k) return k;
    // Row 2
    output_high(PIN_D1); delay_us(20);
    if(input(PIN_D4)) k = '4'; else if(input(PIN_D5)) k = '5'; else if(input(PIN_D6)) k = '6';
    output_low(PIN_D1); if(k) return k;
    // Row 3
    output_high(PIN_D2); delay_us(20);
    if(input(PIN_D4)) k = '7'; else if(input(PIN_D5)) k = '8'; else if(input(PIN_D6)) k = '9';
    output_low(PIN_D2); if(k) return k;
    // Row 4
    output_high(PIN_D3); delay_us(20);
    if(input(PIN_D4)) k = '*'; else if(input(PIN_D5)) k = '0'; else if(input(PIN_D6)) k = '#';
    output_low(PIN_D3);
    return k;
}

// ==================== NG?T KH?N C?P (RB0) ====================
#int_ext
void ext_isr(void) {
    wrong_attempts = 0;
    go_to_unlocked();
}

// ==================== VÒNG L?P CHÍNH ====================
void main() {
    ANSEL = 0x00; ANSELH = 0x00; // Digital mode
    set_tris_b(0x01); // RB0 input (interrupt)
    set_tris_c(0x00); // Port C output
    set_tris_d(0x70); // D0-D3 output (Rows), D4-D6 input (Cols)
    
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
                inactivity_timer += 10;
                if (inactivity_timer >= 5000) { do_action(ACTION_ERROR_BEEP); go_to_idle(); }
                if (key_pressed) { inactivity_timer = 0; process_key_in_typing(key); }
                break;

            case STATE_UNLOCKED:
                inactivity_timer += 10;
                if (inactivity_timer >= 15000) { go_to_idle(); do_action(ACTION_READY_BEEP); }
                else if (inactivity_timer >= 12000 && !reminder_sent) { do_action(ACTION_REMINDER_CHIRP); reminder_sent = 1; }
                if (key_pressed) {
                    if (key == '#') inactivity_timer = 0;
                    else if (key == '*') go_to_idle();
                }
                break;

            case STATE_ALARM:
                for(int i = 0; i < 15; i++) {
                    output_high(RED_LED); do_action(ACTION_ALARM_CHIRP);
                    output_low(RED_LED);  do_action(ACTION_ALARM_CHIRP);
                }
                wrong_attempts = 0;
                go_to_idle();
                break;
        }
        delay_ms(10);
    }
}
