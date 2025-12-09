#include <lvgl.h> //
#include "Arduino_GFX_Library.h"  //1.5.3
#include "lv_conf.h"
//#include <demos/lv_demos.h>
#include <ui.h>
#include "HWCDC.h"
#include "TouchDrvGT911.hpp"
#include <Wire.h>
#include <SPI.h>

TouchDrvGT911 GT911;
int16_t x[5], y[5];
uint8_t gt911_i2c_addr = 0;

float angleX = 1;
float angleY = 0;

bool rotation = false;

HWCDC USBSerial;
#define EXAMPLE_LVGL_TICK_PERIOD_MS 1


enum BoardConstants { GFX_BL=-1, LVGL_BUFFER_RATIO=6 };


static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 480;
//#define LVGL_BUFFER_RATIO 6
enum { SCREENBUFFER_SIZE_PIXELS = screenWidth * LVGL_BUFFER_RATIO };
static lv_color_t *buf;


//uint32_t screenWidth;
//uint32_t screenHeight;

static lv_draw_buf_t draw_buf;
// static lv_color_t buf[screenWidth * screenHeight / 10];

Arduino_DataBus *bus = new Arduino_SWSPI(
  GFX_NOT_DEFINED /* DC */, 42 /* CS */,
  2 /* SCK */, 1 /* MOSI */, GFX_NOT_DEFINED /* MISO */);

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
  40 /* DE */, 39 /* VSYNC */, 38 /* HSYNC */, 41 /* PCLK */,
  46 /* R0 */, 3 /* R1 */, 8 /* R2 */, 18 /* R3 */, 17 /* R4 */,
  14 /* G0 */, 13 /* G1 */, 12 /* G2 */, 11 /* G3 */, 10 /* G4 */, 9 /* G5 */,
  5 /* B0 */, 45 /* B1 */, 48 /* B2 */, 47 /* B3 */, 21 /* B4 */,
  1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
  1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */);
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
  480 /* width */, 480 /* height */, rgbpanel, 2 /* rotation */, true /* auto_flush */,
  bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char *buf) {
  Serial.printf(buf);
  Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *pixelmap)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  //gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  //gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
  gfx->draw16bitRGBBitmap( area->x1, area->y1, (uint16_t*) pixelmap, w, h );
#endif

  lv_disp_flush_ready(disp);
}

void example_increase_lvgl_tick(void *arg) {
  /* Tell LVGL how many milliseconds has elapsed */
  lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static uint8_t count = 0;
void example_increase_reboot(void *arg) {
  count++;
  if (count == 30) {
    esp_restart();
  }
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data) {
  uint8_t touched = GT911.getPoint(x, y, GT911.getSupportTouchPoint());

  if (touched > 0) {
    USBSerial.print(millis());
    USBSerial.print("ms ");
    for (int i = 0; i < touched; ++i) {
      int16_t touchX = x[i];
      int16_t touchY = y[i];
      switch (gfx->getRotation()) {
        case 0:
          break;
        case 1:
          touchX = y[i];
          touchY = gfx->height() - x[i];
          break;
        case 2:
          touchX = gfx->width() - x[i];
          touchY = gfx->height() - y[i];
          break;
        case 3:
          touchX = gfx->width() - y[i];
          touchY = x[i];
          break;
      }
      data->state = LV_INDEV_STATE_PR;

      /*Set the coordinates*/
      data->point.x = touchX;
      data->point.y = touchY;

      USBSerial.print("Data x ");
      USBSerial.print(touchX);

      USBSerial.print("Data y ");
      USBSerial.println(touchY);

      // gfx->fillCircle(touchX, touchY, 5, BLUE);
    }
    USBSerial.println();
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void i2c_scan() {
  USBSerial.println("Scanning I2C bus...");
  byte error, address;
  int nDevices = 0;

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      USBSerial.print("I2C device found at address 0x");
      if (address < 16) {
        USBSerial.print("0");
      }
      USBSerial.println(address, HEX);
      nDevices++;
      
      if (address == GT911_SLAVE_ADDRESS_L || address == GT911_SLAVE_ADDRESS_H) {
        gt911_i2c_addr = address;
        USBSerial.print("Found GT911 candidate address: 0x");
        USBSerial.println(address, HEX);
      }
    } else if (error == 4) {
    }
  }

  if (nDevices == 0) {
    USBSerial.println("No I2C devices found");
  } else {
    USBSerial.println("I2C scan completed");
  }
}

bool init_gt911_with_probe(int sda_pin, int scl_pin) {
  Wire.begin(sda_pin, scl_pin);
  delay(100);
  
  i2c_scan();
  
  if (gt911_i2c_addr == 0) {
    USBSerial.println("GT911 not found in I2C scan");
    return false;
  }
  
  GT911.setPins(-1, -1);
  if (GT911.begin(Wire, gt911_i2c_addr, sda_pin, scl_pin)) {
    USBSerial.print("GT911 initialized successfully at address 0x");
    USBSerial.println(gt911_i2c_addr, HEX);
    return true;
  } else {
    USBSerial.print("Failed to initialize GT911 at address 0x");
    USBSerial.println(gt911_i2c_addr, HEX);
    return false;
  }
}

/*Set tick routine needed for LVGL internal timings*/
static uint32_t my_tick_get_cb (void) { 
  return millis();
}

void setup() {
  USBSerial.begin(115200); /* prepare for possible serial debug */

  Wire.begin(15, 7);

  Wire.beginTransmission(0x24);
  Wire.write(0x03);
  Wire.write(0x3a);
  Wire.endTransmission();

  if (!init_gt911_with_probe(15, 7)) {
    while (1) {
      USBSerial.println("Failed to find GT911 - check your wiring!");
      delay(1000);
    }
  }

  GT911.setHomeButtonCallback([](void *user_data) {
    USBSerial.println("Home button pressed!");
  },
                              NULL);
  GT911.setMaxTouchPoint(1);  // max is 5

  gfx->begin();

  //screenWidth = gfx->width();
  //screenHeight = gfx->height();

  lv_init();

#ifdef ESP32
    buf = (lv_color_t*) heap_caps_malloc( sizeof(lv_color_t) * screenWidth * screenHeight / LVGL_BUFFER_RATIO, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT );
    //static uint16_t buf[480*480 / 10];
    //static uint16_t buf2[480*480 / 10];
    //buf = (lv_color_t *) heap_caps_malloc( sizeof(lv_color_t) * screenWidth * LVGL_BUFFER_RATIO, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT );
#else
    lv_color_t buf = (lv_color_t*) malloc( sizeof(lv_color_t) * screenWidth * screenHeight / LVGL_BUFFER_RATIO );
    //buf = (lv_color_t *) malloc( sizeof(lv_color_t) * screenWidth * LVGL_BUFFER_RATIO );
#endif
    if (!buf) {
        Serial.println("LVGL buf allocate failed!");
    }
    else
    {
        static lv_disp_t* disp;
        disp = lv_display_create( screenWidth, screenHeight );
        lv_display_set_buffers( disp, buf, NULL, SCREENBUFFER_SIZE_PIXELS * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL );
        lv_display_set_flush_cb( disp, my_disp_flush );

        static lv_indev_t* indev;
        indev = lv_indev_create();
        lv_indev_set_type( indev, LV_INDEV_TYPE_POINTER );
        lv_indev_set_read_cb( indev, my_touchpad_read );

        lv_tick_set_cb( my_tick_get_cb );

        ui_init();


  USBSerial.println("Setup done");
    }
}

void loop() {
  lv_timer_handler(); /* let the GUI do its work */
  delay(5);
}
