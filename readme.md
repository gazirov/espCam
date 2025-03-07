# ESP32-Cam Multi-Functional Project
Проект для платформы ESP32-D0WDQ6-V3 с 8 Мб PSRAM и 4 Мб Flash с камерой 3.0 Мп (OV3660) реализует многофункциональное поведение устройства, которое включает:

• Режим точки доступа для первичной конфигурации Wi-Fi через веб-сервер
• Подключение к Wi-Fi и запуск веб-сервера для управления режимами работы камеры
• Два основных режима работы камеры:
1. Потоковая передача видео
2. Интревальная съемка с последующей отправкой снимков
– Отправка по трем каналам: сохранение в папку Samba, отправка через Telegram-бот и публикация в брокер MQTT
• Возможность одновременной работы всех способов отправки данных
• Переход в глубокий сон между интервалами съемки с пробуждением по кнопке на контакте G37
• Получение и синхронизация даты/времени с помощью модуля RTC BM8563 (SCL – G14, SDA – G12) или NTP
• Управление светодиодом на контакте G2 (различное поведение в зависимости от режима:
– Постоянное горение в режиме ожидания Wi-Fi
– 2 раза в секунду при потоковой трансляции
– 4 раза в секунду при интервальной отправке)
• Измерение напряжения батареи (BAT_ADC_Pin на G38 и удержание питания через BAT_HOLD_Pin на G33)
– Формирование имени файла снимка по схеме: ddMMyy_HH:mm_Volt

Проект планируется компилировать и запускаться в Visual Studio Code (с установленными инструментами Arduino или PlatformIO).

## Функциональные возможности
Режим точки доступа для конфигурации Wi-Fi
Пока устройство не подключится к заданной сети, оно создаёт свою точку доступа (AP) с именем "ESP32_Cam_AP" и паролем "12345678". Через веб-интерфейс можно задать нужные параметры для подключения к Wi-Fi-сети.

Подключение к Wi-Fi и управление режимами работы
После подключения к Wi-Fi устройство запускает локальный веб-сервер, позволяющий:

Задать режим работы камеры: потоковая трансляция (режим 1) или интервальная съемка (режим 2).
В режиме интервальной съемки: настроить интервал съемки, выбрать способ отправки (Telegram, MQTT или SMB), задать параметры для Telegram и MQTT.
При интервальной съёмке устройство после каждого снимка уходит в глубокий сон, пробуждаясь либо по истечению заданного интервала, либо по нажатию кнопки на контакте G37.
Потоковая трансляция видео
В случае выбора режима потоковой трансляции (и при отсутствии подключения к заданному Wi-Fi) запускается сервер на порту 81, который передаёт видео с камеры в формате JPEG.

Интевальная передача снимков
При этом режиме камера делает снимки с заданным интервалом, затем:

Отправляет снимок в Telegram через HTTPS-запрос,
Публикует изображение в формате Base64 через MQTT,
(Планируется реализация отправки по протоколу SMB).
Подключение к RTC и NTP
Для получения актуальной даты и времени используется модуль RTC BM8563, подключенный к пинам G12 (SDA) и G14 (SCL). В качестве дополнительного источника времени используется сервер NTP (pool.ntp.org).

Индикатор - светодиод на выводе G2
Светодиод показывает статус устройства:

Горит постоянно, если устройство находится в режиме ожидания подключения к Wi-Fi.
Моргает дважды в секунду в режиме потоковой трансляции.
Моргает четыре раза в секунду при интервальной отправке данных.
Измерение уровня напряжения батареи
С помощью аналогового входа BAT_ADC_Pin измеряется напряжение батареи, информация о котором включается в имя файла при сохранении снимка.

## Используемые библиотеки и зависимости
Проект использует следующие библиотеки (установку можно выполнить через Library Manager Arduino IDE или PlatformIO):

WiFi.h
WebServer.h и ESPAsyncWebServer.h (с AsyncTCP) для работы веб-сервера
SPIFFS.h (для работы с файловой системой)
ArduinoJson.h (для сериализации/десериализации настроек)
Wire.h (для работы с I2C)
RTClib.h (для работы с RTC)
esp_camera.h (библиотека для работы с камерой ESP32)
time.h и WiFiUdp.h (для NTP синхронизации)
PubSubClient.h (для MQTT)
DNSServer.h, HTTPClient.h, WiFiClientSecure.h и другие используемые для отправки данных
Убедитесь, что установлены все требуемые библиотеки и выбран корректный пакет для компиляции под ESP32.

Аппаратное подключение
Ниже приведены основные подключения:

Камера OV3660:
<details>
<summary>hardware</summary>
• SCCB Clock: IO23
• SCCB Data: IO25
• System Clock: IO27
• VSYNC: IO22
• HREF: IO26
• PCLK: IO21
• Pixel Data Bits:
– D0: IO32
– D1: IO35
– D2: IO34
– D3: IO5
– D4: IO39
– D5: IO18
– D6: IO36
– D7: IO19
• Camera Reset: IO15
• Camera PWDN: не используется (-1)

RTC BM8563:
• SDA: G12
• SCL: G14

Светодиод:
• G2

Батарея:
• BAT_ADC_Pin: G38
• BAT_HOLD_Pin: G33
</details>
Пробуждение по кнопке:
• Кнопка подключена к контакту G37 (дополнительно настроено через RTC GPIO)

Убедитесь, что схема подключения соответствует указанной раскладке пинов.

## Сборка и установка через Visual Studio Code
Установите Visual Studio Code и необходимые расширения для работы с Arduino или PlatformIO.
Склонируйте репозиторий проекта:
Откройте проект в VS Code.
Убедитесь, что настройки сборки (platformio.ini или настройки Arduino) соответствуют используемой плате ESP32-D0WDQ6-V3.
Проверьте наличие всех необходимых библиотек и, при необходимости, установите их через Library Manager.
Скомпилируйте проект и загрузите его на плату с помощью встроенных средств VS Code.

## Настройка устройства
После загрузки проекта устройство автоматически запускается в режиме точки доступа, если не может подключиться к сохранённой Wi-Fi сети. Перейдите на созданную точку доступа (SSID "ESP32_Cam_AP", password "12345678") и откройте веб-браузер по адресу http://192.168.4.1. На открывшейся странице вы сможете:

Указать SSID и пароль вашей сети Wi-Fi
Выбрать режим работы (поток, интервальная съемка)
Настроить параметры Telegram, MQTT и интервал съемки
После сохранения настроек устройство перезагрузится и попробует подключиться к указанной Wi-Fi-сети. В зависимости от выбранного режима работы устройство запустит соответствующий веб-сервер или выполнит отправку снимков и перейдёт в глубокий сон.

## Заметки
При отправке изображения через Telegram или MQTT обязательно проверьте корректность настроек и учётных данных.
Отправка через Samba пока не реализована и требует дальнейшей доработки.
При использовании глубокого сна устройство предварительно деинициализирует камеру для экономии энергии.