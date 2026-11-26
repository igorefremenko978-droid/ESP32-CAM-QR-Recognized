#include <Arduino.h>
#include <ESP32QRCodeReader.h>

// ==========================================
// МОДУЛЬ 1: КОНФИГУРАЦИЯ И ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ==========================================
// ВАЖНО: Ваша плата ESP32-CAM должна иметь PSRAM (обычно AI-Thinker)
ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER);

// Настройки фильтрации
const int MISSING_THRESHOLD = 5; // Сколько кадров подряд QR должен отсутствовать, чтобы считать его убранным
int missingCounter = 0;          // Текущий счетчик пропусков
bool isQrArmed = true;           // Флаг "готовность принять новый код" (взведен)

// Хранилище последнего успешно отправленного payload
String lastPayload = ""; 

// Флаг для отслеживания, был ли уже отправлен текущий код
bool isCurrentCodeSent = false;

// ==========================================
// МОДУЛЬ 2: ФУНКЦИЯ ОТПРАВКИ В UART
// ==========================================
void sendUartData(const String &payload) {
  // Здесь можно изменить формат вывода, если нужно (например, добавить префикс)
  Serial.print("QR_DATA: ");
  Serial.println(payload);
}

// ==========================================
// МОДУЛЬ 3: ОСНОВНАЯ ЗАДАЧА СКАНИРОВАНИЯ
// ==========================================
void onQrCodeTask(void *pvParameters) {
  struct QRCodeData qrCodeData;
  bool wasVisible = false;  // было ли что-то в кадре на прошлом шаге

  while (true) {
    bool frameHasQr = reader.receiveQrCode(&qrCodeData, 100);

    // --- Отладочная индикация состояния кадра ---
    if (frameHasQr && !wasVisible) {
      Serial.println("[CAM] QR появился в кадре");
      wasVisible = true;
    } else if (!frameHasQr && wasVisible) {
      // ещё не факт, что QR убрали — просто кадр не распознан
      // реальное "исчез" определяется по missingCounter
    }

    if (frameHasQr) {
      missingCounter = 0;

      if (qrCodeData.valid) {
        String currentPayload = String((const char *)qrCodeData.payload);

        if (!isCurrentCodeSent || (currentPayload != lastPayload)) {
          if (currentPayload == lastPayload && !isQrArmed) {
            // дубль, игнорируем
          } else {
            sendUartData(currentPayload);
            lastPayload = currentPayload;
            isCurrentCodeSent = true;
            isQrArmed = false;
          }
        }
      }
    } else {
      if (missingCounter < MISSING_THRESHOLD) {
        missingCounter++;
      }

      if (missingCounter >= MISSING_THRESHOLD) {
        if (!isQrArmed) {
          isQrArmed = true;
          isCurrentCodeSent = false;
          wasVisible = false;
          Serial.println("[CAM] QR исчез из кадра (готов к новому)");
        }
      }
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ==========================================
// МОДУЛЬ 4: НАСТРОЙКА (SETUP)
// ==========================================
void setup() {
  // Инициализация UART (Serial) для передачи данных
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== ESP32-CAM QR Scanner Started ===");

  Serial.setDebugOutput(false); // это отключит отладочный вывод ядра ESP32, но не весь вывод библиотеки.

  // Инициализация ридера
  reader.setup();
  Serial.println("QR Reader setup done.");

  // Запуск ридера на ядре 1 (стандартная практика для ESP32)
  reader.beginOnCore(1);
  Serial.println("QR Reader started on Core 1.");

  // Создание задачи для сканирования в фоне
  // Это позволяет loop() оставаться свободным для других задач, если понадобятся
  xTaskCreate(
    onQrCodeTask,   // Функция задачи
    "onQrCode",     // Имя задачи
    4 * 1024,       // Размер стека
    NULL,           // Параметры
    4,              // Приоритет
    NULL            // Дескриптор
  );
  Serial.println("Task created. Waiting for QR codes...");
}

// ==========================================
// МОДУЛЬ 5: ОСНОВНОЙ ЦИКЛ (LOOP)
// ==========================================
void loop() {
  // Основной цикл пуст, так как вся работа происходит в задаче onQrCodeTask.
  // Это хорошая архитектура для ESP32, позволяющая избежать блокировок.
  delay(1000); 
}
