#include <U8g2lib.h>
#include <Wire.h>

// OLED - usa el modo ligero (buffer parcial)
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#define WAVE_SIZE 64
volatile uint8_t waveTable[WAVE_SIZE];  // RAM optimizada
volatile uint8_t waveIndex = 0;

uint8_t scopeData[WAVE_SIZE];  // solo para dibujar

// Pines de potenciómetros
#define POT_FREQ     A0
#define POT_VOL      A1
#define POT_SHAPE    A2

void setup() {
  // Configura PWM en D9 (OC1A)
  pinMode(9, OUTPUT);
  setupPWM();

  // Inicializa pantalla
  u8g2.begin();

  // Timer2 para actualizaciones de onda
  setupTimer2();

  // Llenar tabla inicial
  generateWaveform();
}

void loop() {
  readScopeData();          // Copia datos de waveTable para dibujar
  drawScope();              // Dibuja la forma de onda
  generateWaveform();       // Actualiza forma de onda con potes
  delay(10);                // Pequeño delay para fluidez
}

// PWM con Timer1 para salida en D9
void setupPWM() {
  TCCR1A = _BV(COM1A1) | _BV(WGM11);     // PWM modo 14
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10); // Sin prescaler
  ICR1 = 255;
  OCR1A = 128; // Nivel inicial medio
}

// Timer2: genera interrupciones a frecuencia variable
void setupTimer2() {
  cli();
  TCCR2A = _BV(WGM21);       // CTC
  TCCR2B = _BV(CS20);        // sin prescaler
  OCR2A = 125;               // valor inicial
  TIMSK2 = _BV(OCIE2A);      // habilita interrupción
  sei();
}

// ISR para generar señal en D9
ISR(TIMER2_COMPA_vect) {
  OCR1A = waveTable[waveIndex];
  waveIndex++;
  if (waveIndex >= WAVE_SIZE) waveIndex = 0;
}

// Genera una forma de onda nueva según potenciómetros
void generateWaveform() {
  int freqVal = analogRead(POT_FREQ);    // 0 - 1023
  int volVal  = analogRead(POT_VOL);     // 0 - 1023
  int shapeVal= analogRead(POT_SHAPE);   // 0 - 1023

  // Calcula nueva frecuencia
  int freq = map(freqVal, 0, 1023, 50, 1000); // Hz
  updateTimerForFrequency(freq);

  // Calcula volumen (0 - 255)
  uint8_t volume = map(volVal, 0, 1023, 0, 255);

  // Forma de onda: de seno a diente
  for (uint8_t i = 0; i < WAVE_SIZE; i++) {
    float angle = (2 * PI * i) / WAVE_SIZE;
    float sine = sin(angle) * 0.5 + 0.5;
    float saw  = (float)i / WAVE_SIZE;
    float blend = (shapeVal / 1023.0); // 0.0 a 1.0
    float wave = sine * (1.0 - blend) + saw * blend;
    waveTable[i] = (uint8_t)(wave * volume);
  }
}

// Ajusta Timer2 para frecuencia deseada
void updateTimerForFrequency(uint16_t freq) {
  uint16_t top = (F_CPU / (WAVE_SIZE * freq)) - 1;
  if (top < 20) top = 20;
  if (top > 255) top = 255;
  cli();
  OCR2A = top;
  sei();
}

// Copia datos actuales para dibujarlos (protegido)
void readScopeData() {
  cli();
  for (uint8_t i = 0; i < WAVE_SIZE; i++) {
    scopeData[i] = waveTable[i];
  }
  sei();
}

// Dibuja forma de onda
void drawScope() {
  u8g2.firstPage();
  do {
    for (uint8_t i = 1; i < WAVE_SIZE; i++) {
      int x0 = (i - 1) * 2;
      int y0 = 64 - (scopeData[i - 1] >> 2);
      int x1 = i * 2;
      int y1 = 64 - (scopeData[i] >> 2);
      u8g2.drawLine(x0, y0, x1, y1);
    }
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.setCursor(0,10);
    u8g2.print("F:");
    u8g2.print(map(analogRead(POT_FREQ), 0, 1023, 50, 1000));
    u8g2.print("Hz  V:");
    u8g2.print(map(analogRead(POT_VOL), 0, 1023, 0, 255));
  } while (u8g2.nextPage());
}
