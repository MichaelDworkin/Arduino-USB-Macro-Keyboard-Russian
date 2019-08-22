// Резистивная сенсорная панель в качестве USB Macro Keypad
// программируемая клавиатура для горячих клавиш
// Свободно программируемая USB-клавиатура для макросов
// Arduino Leonardo Micro + сенсорная панель
// Поддержка английского и русского языков

// Страница проекта:  http://esp8266-server.de/keypadRU.html

// Автор: Михаил Дворкин

// формат команд: "§"- Начало команды, Команда, Партаметр, пробел -Конец команды  (Пример "слово1§t1000 цслово2" эта команда означает жди 1 секунду )
// §p - Press         "§p0x80 "  нажать Strg
// §r - Release       "§r0x80 "  отпустить Strg
// §a - Release All   "§a "      отпустить все нажатые клавиши
// §w - Write         "§w0xB0 "  напечатать ввод (enter)
// §t - Timer         "§t3000  " 3 секунды ждать
// §l - Sprache       "§l0  "   переключит на 0-английский 1-русский
// Скан-коды смотрите здесь
// https://github.com/MichaelDworkin/KeyboardMultiLanguage/blob/master/src/KeyboardMultiLanguage.h

#include <stdint.h>
#include <EEPROM.h>
#include <TouchScreen.h>
#include <KeyboardMultiLanguage.h>
#include "KeyboardMappingRU.h"  // Таблица русской раскладки


// ------------------- Распиновка Touchscreen Panel ----------------
#define X1 A1
#define X2 A3
#define Y1 A0
#define Y2 A2

/*
  #define Y1 A1
  #define Y2 A3
  #define X1 A0
  #define X2 A2
*/

#define PIEZO_PIN 3  // номер ножки с Пьезо

int kalibriere = 0;
boolean losgelassen = 1;
int KalibrWert[4];

// ----------------- Пожалуйста, здесь введите количество столбцов и строк  -----------------

#define Reihen 3  // строк
#define Spalten 4 // столбцов

// ----------------- Команды и тексты, присвоенны соответствующей ячейке -------------------
// ----------------- Двумерный массив соответствует таблице клавиш ----------------------

// открытие скайпа, поиск абонента, видеовызов
//#define skype "§p0x87 r§r0x87 §t100 \"C:\\Program Files (x86)\\Microsoft\\Skype for Desktop\\Skype.exe\"\n§t3000 §p0x80 §p0x81 s§t500 §a §t500 echo§t3000 §w0xD9 \n§p0x80 §p0x81 k§a "
// открытие ардуино, открытие пследнего проекта, закрытие пустого окна
//#define arduino "§p0x87 r§r0x87 §t100 \"C:\\arduino-1.8.8\\arduino.exe\"\n§t12000 §w0x82 §t100 §w0xD9 §t100 §w0xD9 §t100 §w0xD9 §t100 §w0xD7 §t100 §w0xB0 §t500 §p0x82 §t100 §w0xb3 §t100 §r0x82 §p0x82 §p0xc5 §a "

const String data[Reihen][Spalten] =
{
  { "§l0 Test\n", "\\", "", "§p0x80 e§r0x80 "},
  { "Sehr geherte Damen und Herren,\n", "Michael Dworkin\nReichstr. 18\n42281 Wuppertal", "Mit freundlichen Grüßen\nMichael Dworkin", "Bitte Bestellen" },
  { "1", "§l0 §p0x83 §t100 r§r0x83 §t1000 notepad.exe\n§t1000 §p0x83 §t100 §w0xDA §r0x83 ", "§p0x83  §r0x83 §t500 §l0 I can eat glass and it doesn't hurt me.\n\n", "§p0x83  §r0x83 §t500 §l1 Я могу есть стекло, оно мне не вредит.\n\n§p0x83  §r0x83 §t500 "},
};
/*
  const String data[Reihen][Spalten]
  {
  { "1", "2", "3", "A" },
  { "4", "5", "6", "B" },
  { "7", "8", "9", "C"},
  };
*/


// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate

TouchScreen ts = TouchScreen(X1, Y1, X2, Y2, 300);


// ---------------- преобразование HEX String в Integer -----------------------

unsigned int hexToDec(String hexString)
{
  unsigned int decValue = 0;
  int nextInt;
  hexString.toUpperCase();
  for (int i = 0; i < hexString.length(); i++)
  {
    nextInt = int(hexString.charAt(i));
    if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
    if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
    if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
    nextInt = constrain(nextInt, 0, 15);
    decValue = (decValue * 16) + nextInt;
  }
  return decValue;
}

// ---------------- Setup ------------------------------------------

void setup(void)
{
  Serial.begin(115200);
  pinMode(PIEZO_PIN, OUTPUT);
  Keyboard.language(Russian); // выбор языка /раскладки
  delay(1000);
  EEPROM.get( 0 , KalibrWert ); // считываем из памяти значения калибровки сенсорной панельи
  if (KalibrWert[0] >= KalibrWert[1]  || KalibrWert[2] >= KalibrWert[3]) // если дурацкие значения
  {
    KalibrWert[0] = 0;                                                   // то загружаем значения по умолчанию
    KalibrWert[1] = 1000;
    KalibrWert[2] = 0;
    KalibrWert[3] = 1000;
  }
}

// ---------- Вывод текста через HID-клавиатуру и выполнение команд -----

void KeyOutput(const String str)
{
  int pos = str.indexOf('§');   // находим начало первой команды
  while (pos >= 0)
  {
    if (pos > 0) Keyboard.print(str.substring(0, pos - 1)); // выводим текст до команды и между командами
    str.remove(0, pos + 1);
    char kode = str.charAt(0); // выделяем команды
    pos = str.indexOf(' ');
    String Daten = str.substring(1, pos); // выделяем параметр
    byte z;
    switch (kode)             // команды
    {
      case 'w':
        z = hexToDec(Daten);
        Keyboard.write(z);
        break;
      case 't':
        delay(Daten.toInt());
        break;
      case 'p':
        z = hexToDec(Daten);
        Keyboard.press(z);
        break;
      case 'r':
        z = hexToDec(Daten);
        Keyboard.release(z);
        break;
      case 'a':
        Keyboard.releaseAll();
        break;
      case 'l':
        if (Daten == "1") Keyboard.language(Russian);
        else Keyboard.language();
        break;
    }
    str.remove(0, pos + 1);
    pos = str.indexOf('§'); // находим начало следующей команды
  }
  if (str.length() > 0)Keyboard.print(str); // выводим текст после команды
}

void loop(void) {
  TSPoint p = ts.getPoint(); // a point object holds x y and z coordinates

  // we have some minimum pressure we consider 'valid'
  if (p.z < ts.pressureThreshhold) losgelassen = 1; // запоминаем отпускание сенсора
  if (p.z > ts.pressureThreshhold && losgelassen)   // если нажато а раньше было отпущено то
  {
    int x = map( p.x, KalibrWert[0], KalibrWert[1], 0, Spalten);  // вычесляем столбец
    int y = map( p.y, KalibrWert[2], KalibrWert[3], 0, Reihen);   // вычесляем строку
    if (y < Reihen && x < Spalten)
    {
      digitalWrite(PIEZO_PIN, !digitalRead(PIEZO_PIN));  // издаём щелчок
      if (!kalibriere) KeyOutput(data[y][x]);            // если не в модусе отладки то печатаем данные
    }
    else Serial.print("номер строки или столбца вне массива данных!");

    losgelassen = 0;   // сенсор до сих пор нажат
    delay(300);

    switch (kalibriere)             // контроль выбора калибровки и отладки
    {
      case 1:
        kalibriere = 2;
        KalibrWert[0] = p.x; // x min
        KalibrWert[2] = p.y; // y min
        delay(1000);
        Serial.println("в нижнем правом углу");
        break;
      case 2:
        kalibriere = 0;
        Serial.println("");
        KalibrWert[1] = p.x; // x max
        KalibrWert[3] = p.y; // y max
        delay(1000);
        Serial.println("калибровка окончина");
        EEPROM.put( 0, KalibrWert);
        Serial.println("X Min \tX Max \tY Min \tY Max");
        Serial.print(String(KalibrWert[0]) + "\t");
        Serial.print(String(KalibrWert[1]) + "\t");
        Serial.print(String(KalibrWert[2]) + "\t");
        Serial.println(String(KalibrWert[3]) + "\t");
        delay(1000);
        break;
      case 3:
        Serial.println("значения аналого-цифрового преобразователя:");
        Serial.print("X = "); Serial.print(p.x);
        Serial.print("\tY = "); Serial.print(p.y);
        Serial.println("  ");
        Serial.println("откалиброванные и пересчитанные в таблицу значения:");
        Serial.print("X = ");
        Serial.print(x);
        Serial.print("\tY = ");
        Serial.println(y);
        Serial.println("  ");
        break;
    }

  }


  // 2 команды через последовательный терминал
  // символ "k" - калибровка
  // символ "r" - показывает абсолютные значения положения и значения сетки

  if (Serial.available())           // Когда ввод сделан
  {
    char  Wahl = Serial.read();     // Получить введенный знак
    Serial.flush();
    Serial.println(Wahl);    
    switch (Wahl)                   // контроль выбора
    {
      case 'k':                     // старт калибровки
        kalibriere = 1;
        Serial.println("калибровка");
        Serial.println("Нажмите:");
        Serial.println("В верхнем левом углу");
        break;
      case 'r':                   // старт вывода отладочной информации
        if (!kalibriere)
        {
          kalibriere = 3;
          Serial.println("Значения положения и значения сетки");
          Serial.println("чтобы вернутся нажмите \"r\"");
        }
        else
        {
          kalibriere = 0;
          Serial.println("USB-Клавиатура снова включина");
        }
        break;
    }
  }
}
