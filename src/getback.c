#include <pebble.h>
#undef APP_LOG
#define APP_LOG(...)

static Window *window;
static TextLayer *distance_layer;
static TextLayer *unit_layer;
static TextLayer *hint_layer;
static Layer *head_layer;
static TextLayer *n_layer;
static TextLayer *e_layer;
static TextLayer *s_layer;
static TextLayer *w_layer;
static TextLayer *heading_layer;
static TextLayer *direction_layer;
static TextLayer *bearing_layer;
static TextLayer *accuracy_layer;
static TextLayer *speed_layer;
int32_t distance = 0;
int16_t bearing = 0;
int16_t heading = 0;
int16_t orientation = 0;
int16_t interval = 5;
static const uint32_t CMD_KEY = 0x1;
static const uint32_t BEARING_KEY = 0x2;
static const uint32_t DISTANCE_KEY = 0x3;
static const uint32_t UNIT_KEY = 0x4;
static const uint32_t INTERVAL_KEY = 0x5;
static const uint32_t SPEED_KEY = 0x6;
static const uint32_t ACCURACY_KEY = 0x7;
static const uint32_t HEADING_KEY = 0x8;
static char command[6] = "get";
static GPath *head_path;
static GRect hint_layer_size;
static AppTimer *locationtimer;
bool updated = true;
GPoint center;

const GPathInfo HEAD_PATH_POINTS = {
  3,
  (GPoint []) {
    {0, -77},
    {-11, -48},
    {11,  -48},
  }
};

static void tick_handler(void *data) {
  if (interval == 0) return;
  text_layer_set_text(speed_layer, "?");
  strcpy(command,"get");
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {
    vibes_short_pulse();
    APP_LOG(APP_LOG_LEVEL_WARNING, "Can not send command '%s' to phone!", command);
  } else {
    dict_write_cstring(iter, CMD_KEY, command);
    const uint32_t final_size = dict_write_end(iter);
    app_message_outbox_send();
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent command '%s' to phone! (%d bytes)", command, (int) final_size);
  }
  static char interval_buf[8];
  snprintf(interval_buf, sizeof(interval_buf), "%ds", interval);
  text_layer_set_text(accuracy_layer, interval_buf);
  
  locationtimer = app_timer_register(1000 * interval, (AppTimerCallback) tick_handler, NULL);
}
  
static void reset_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Reset");
  if (hint_layer) {
    text_layer_destroy(hint_layer);
    hint_layer = NULL;
  }
  app_timer_reschedule(locationtimer, 1000 * (interval + 7));
  strcpy(command, "set");
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {
    vibes_short_pulse();
    APP_LOG(APP_LOG_LEVEL_WARNING, "Can not send command %s to phone!", command);
    return;
  }
  dict_write_cstring(iter, CMD_KEY, command);
  const uint32_t final_size = dict_write_end(iter);
  app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent command '%s' to phone! (%d bytes)", command, (int) final_size);
}

static void hint_handler(ClickRecognizerRef recognizer, void *context) {
  if (hint_layer) {
    text_layer_destroy(hint_layer);
    hint_layer = NULL;
  } else {
    tick_handler(NULL);
//    Layer *window_layer = window_get_root_layer(window);
//    hint_layer = text_layer_create(hint_layer_size);
//    text_layer_set_font(hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
//    text_layer_set_text_alignment(hint_layer, GTextAlignmentCenter);
//    layer_add_child(window_layer, text_layer_get_layer(hint_layer));
//    text_layer_set_text(hint_layer, "Press and hold to set target to current position.");
  }  
}  

void out_sent_handler(DictionaryIterator *sent, void *context) {
   // outgoing message was delivered
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Message accepted by phone: %s", command);
  if (strcmp(command,"set")==0) {
    Layer *window_layer = window_get_root_layer(window);
    hint_layer = text_layer_create(hint_layer_size);
    text_layer_set_font(hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(hint_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(hint_layer));
    text_layer_set_text(hint_layer, "Target set. Press again to continue.");
    vibes_long_pulse();
  };
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
   // outgoing message failed
  APP_LOG(APP_LOG_LEVEL_WARNING, "Message rejected by phone: %s %d", command, reason);
  if (reason == APP_MSG_SEND_TIMEOUT) {
    if (strcmp(command,"set")==0) {
      Layer *window_layer = window_get_root_layer(window);
      hint_layer = text_layer_create(hint_layer_size);
      text_layer_set_font(hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
      text_layer_set_text_alignment(hint_layer, GTextAlignmentCenter);
      layer_add_child(window_layer, text_layer_get_layer(hint_layer));
      text_layer_set_text(hint_layer, "Cannot set target. Please restart the app.");
    };
  }
//  vibes_double_pulse();
}

void in_received_handler(DictionaryIterator *iter, void *context) {
  // incoming message received
  static char unit_text[9];
  static char distance_text[9];
  static char bearing_text[9];
  static char heading_text[9];
  static char speed_text[9];
  static char accuracy_text[12];
  
  updated = true;
  
  Tuple *bearing_tuple = dict_find(iter, BEARING_KEY);
  if (bearing_tuple) {
    bearing = bearing_tuple->value->int16;
    snprintf(bearing_text, sizeof(bearing_text), "%d", bearing);
    text_layer_set_text(bearing_layer, bearing_text);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Updated bearing to %d", bearing);
    layer_mark_dirty(head_layer);
  }

  Tuple *unit_tuple = dict_find(iter, UNIT_KEY);
  if (unit_tuple) {
    strcpy(unit_text, unit_tuple->value->cstring);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Unit: %s", unit_text);
  }
  text_layer_set_text(unit_layer, unit_text);

  Tuple *distance_tuple = dict_find(iter, DISTANCE_KEY);
  if (distance_tuple) {
    strcpy(distance_text, distance_tuple->value->cstring);
   }
  text_layer_set_text(distance_layer, distance_text);
  

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Distance updated: %s %s", dist_text, text_layer_get_text(unit_layer));
  
  Tuple *interval_tuple = dict_find(iter, INTERVAL_KEY);
  if (interval_tuple) {
    interval = interval_tuple->value->int32;
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG,"New interval is %d.", interval);
 
  Tuple *speed_tuple = dict_find(iter, SPEED_KEY);
  if (speed_tuple) {
    strcpy(speed_text, speed_tuple->value->cstring);
    text_layer_set_text(speed_layer, speed_text);
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Speed is %s.", speed_text);
  }
  
  Tuple *accuracy_tuple = dict_find(iter, ACCURACY_KEY);
  if (accuracy_tuple) {
    strcpy(accuracy_text, accuracy_tuple->value->cstring);
    text_layer_set_text(accuracy_layer, accuracy_text);
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Accuracy is %s.", accuracy_text);
  }
  
  Tuple *heading_tuple = dict_find(iter, HEADING_KEY);
  if (heading_tuple) {
    heading = heading_tuple->value->int16;
    if (heading >= 0) {
      snprintf(heading_text, sizeof(heading_text), "%d", heading);
      text_layer_set_text(heading_layer, heading_text);
    } else
      text_layer_set_text(heading_layer, "");
      
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Updated heading to %d", heading);
  }
}

void compass_direction_handler(CompassHeadingData direction_data){
  static char direction_buf[]="~~~";
  orientation = direction_data.true_heading;
  switch (direction_data.compass_status) {
    case CompassStatusDataInvalid:
      snprintf(direction_buf, sizeof(direction_buf), "%s", "!!!");
      return;
    case CompassStatusCalibrating:
      snprintf(direction_buf, sizeof(direction_buf), "%s", "???");
      break;
    case CompassStatusCalibrated:
      snprintf(direction_buf, sizeof(direction_buf), "%d", (int) ((360-TRIGANGLE_TO_DEG(orientation)) % 360));
  }
  layer_mark_dirty(head_layer);
  text_layer_set_text(direction_layer, direction_buf);
  int32_t nx = center.x + 63 * sin_lookup(orientation)/TRIG_MAX_RATIO;
  int32_t ny = center.y - 63 * cos_lookup(orientation)/TRIG_MAX_RATIO;
  layer_set_frame((Layer *) n_layer, GRect(nx - 14, ny - 18, 28, 28));
  int32_t ex = center.x + 63 * sin_lookup(orientation + TRIG_MAX_ANGLE/4)/TRIG_MAX_RATIO;
  int32_t ey = center.y - 63 * cos_lookup(orientation + TRIG_MAX_ANGLE/4)/TRIG_MAX_RATIO;
  layer_set_frame((Layer *) e_layer, GRect(ex - 14, ey - 18, 28, 28));
  int32_t sx = center.x + 63 * sin_lookup(orientation + TRIG_MAX_ANGLE/2)/TRIG_MAX_RATIO;
  int32_t sy = center.y - 63 * cos_lookup(orientation + TRIG_MAX_ANGLE/2)/TRIG_MAX_RATIO;
  layer_set_frame((Layer *) s_layer, GRect(sx - 14, sy - 18, 28, 28));
  int32_t wx = center.x + 63 * sin_lookup(orientation + TRIG_MAX_ANGLE*3/4)/TRIG_MAX_RATIO;
  int32_t wy = center.y - 63 * cos_lookup(orientation + TRIG_MAX_ANGLE*3/4)/TRIG_MAX_RATIO;
  layer_set_frame((Layer *) w_layer, GRect(wx - 14, wy - 18, 28, 28));
}

static void head_layer_update_callback(Layer *layer, GContext *ctx) {
  gpath_rotate_to(head_path, (TRIG_MAX_ANGLE / 360) * (bearing + TRIGANGLE_TO_DEG(orientation)));
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, center, 77);
  graphics_context_set_fill_color(ctx, GColorWhite);
  gpath_draw_filled(ctx, head_path);
  graphics_fill_circle(ctx, center, 49);
  graphics_context_set_fill_color(ctx, GColorBlack);
}

void in_dropped_handler(AppMessageResult reason, void *context) {
   // incoming message dropped
  APP_LOG(APP_LOG_LEVEL_WARNING, "Could not handle message from watch: %d", reason);
}
 
static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, hint_handler);
//  window_single_click_subscribe(BUTTON_ID_UP, hint_handler);
//  window_single_click_subscribe(BUTTON_ID_DOWN, hint_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, reset_handler, NULL);
//  window_long_click_subscribe(BUTTON_ID_UP, 0, reset_handler, NULL);
//  window_long_click_subscribe(BUTTON_ID_DOWN, 0, reset_handler, NULL);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  center = grect_center_point(&bounds);

  head_layer = layer_create(bounds);
  layer_set_update_proc(head_layer, head_layer_update_callback);
  head_path = gpath_create(&HEAD_PATH_POINTS);
  gpath_move_to(head_path, grect_center_point(&bounds));  
  layer_add_child(window_layer, head_layer);

  n_layer = text_layer_create(GRect(center.x-14, center.y-80, 28, 28));
  text_layer_set_background_color(n_layer, GColorClear);
  text_layer_set_text_color(n_layer, GColorWhite);
  text_layer_set_font(n_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(n_layer, GTextAlignmentCenter);
  text_layer_set_text(n_layer, "N");
  layer_add_child(head_layer, (Layer *) n_layer);

  e_layer = text_layer_create(GRect(center.x+48, center.y-19, 28, 28));
  text_layer_set_background_color(e_layer, GColorClear);
  text_layer_set_text_color(e_layer, GColorWhite);
  text_layer_set_font(e_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(e_layer, GTextAlignmentCenter);
  text_layer_set_text(e_layer, "E");
  layer_add_child(head_layer, (Layer *) e_layer);

  s_layer = text_layer_create(GRect(center.x-14, center.y+48, 28, 28));
  text_layer_set_background_color(s_layer, GColorClear);
  text_layer_set_text_color(s_layer, GColorWhite);
  text_layer_set_font(s_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_layer, GTextAlignmentCenter);
  text_layer_set_text(s_layer, "S");
  layer_add_child(head_layer, (Layer *) s_layer);

  w_layer = text_layer_create(GRect(center.x-76, center.y-18, 28, 28));
  text_layer_set_background_color(w_layer, GColorClear);
  text_layer_set_text_color(w_layer, GColorWhite);
  text_layer_set_font(w_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(w_layer, GTextAlignmentCenter);
  text_layer_set_text(w_layer, "W");
  layer_add_child(head_layer, text_layer_get_layer(w_layer));

  distance_layer = text_layer_create(GRect(0, 48, 144, 42));
  text_layer_set_background_color(distance_layer, GColorClear);
  text_layer_set_font(distance_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(distance_layer, GTextAlignmentCenter);
  text_layer_set_text(distance_layer, "...");
  layer_add_child(window_layer, text_layer_get_layer(distance_layer));
  
  unit_layer = text_layer_create(GRect(50, 84, 44, 30));
  text_layer_set_background_color(unit_layer, GColorClear);
  text_layer_set_font(unit_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(unit_layer, GTextAlignmentCenter);
  text_layer_set_text(unit_layer, "...");
  layer_add_child(window_layer, text_layer_get_layer(unit_layer));

  direction_layer = text_layer_create(GRect(90, 132, 53, 18));
  text_layer_set_background_color(direction_layer, GColorClear);
  text_layer_set_font(direction_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(direction_layer, GTextAlignmentRight);
  text_layer_set_text(direction_layer, "...");
  layer_add_child(window_layer, text_layer_get_layer(direction_layer));

  bearing_layer = text_layer_create(GRect(0, 132, 53, 18));
  text_layer_set_background_color(bearing_layer, GColorClear);
  text_layer_set_font(bearing_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(bearing_layer, GTextAlignmentLeft);
  text_layer_set_text(direction_layer, "...");
  layer_add_child(window_layer, text_layer_get_layer(bearing_layer));

  speed_layer = text_layer_create(GRect(40, 26, 64, 30));
  text_layer_set_background_color(speed_layer, GColorClear);
  text_layer_set_font(speed_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(speed_layer, GTextAlignmentCenter);
  text_layer_set_text(speed_layer, "...");
  layer_add_child(window_layer, text_layer_get_layer(speed_layer));

  accuracy_layer = text_layer_create(GRect(90, -4, 53, 18));
  text_layer_set_background_color(accuracy_layer, GColorClear);
  text_layer_set_font(accuracy_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(accuracy_layer, GTextAlignmentRight);
  text_layer_set_text(accuracy_layer, "...");
  layer_add_child(window_layer, text_layer_get_layer(accuracy_layer));

  heading_layer = text_layer_create(GRect(0, -4, 53, 18));
  text_layer_set_background_color(heading_layer, GColorClear);
  text_layer_set_font(heading_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(heading_layer, GTextAlignmentLeft);
  text_layer_set_text(heading_layer, "...");
  layer_add_child(window_layer, text_layer_get_layer(heading_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(heading_layer);
  text_layer_destroy(speed_layer);
  text_layer_destroy(accuracy_layer);
  text_layer_destroy(bearing_layer);
  text_layer_destroy(direction_layer);
  text_layer_destroy(unit_layer);
  text_layer_destroy(distance_layer);
  text_layer_destroy(w_layer);
  text_layer_destroy(s_layer);
  text_layer_destroy(e_layer);
  text_layer_destroy(n_layer);
  if (hint_layer) {
    text_layer_destroy(hint_layer);
  }
}

static void init(void) {
  // update screen only when heading changes at least 3 degrees
  compass_service_set_heading_filter(TRIG_MAX_ANGLE*3/360);
  compass_service_subscribe(&compass_direction_handler);
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_register_outbox_sent(out_sent_handler);
  app_message_register_outbox_failed(out_failed_handler);
  hint_layer_size = GRect(10, 30, 124, 84);
  const uint32_t inbound_size = APP_MESSAGE_INBOX_SIZE_MINIMUM;
  const uint32_t outbound_size = APP_MESSAGE_OUTBOX_SIZE_MINIMUM;
  app_message_open(inbound_size, outbound_size);
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);

  // Register with TickTimerService
  // tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  // locationtimer = app_timer_register	(	1000 * interval, (AppTimerCallback) tick_handler, NULL);
  
    locationtimer = app_timer_register	(	5000, (AppTimerCallback) tick_handler, NULL);
}

static void deinit(void) {
  app_timer_cancel(locationtimer);
  compass_service_unsubscribe();
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {

    APP_LOG(APP_LOG_LEVEL_WARNING, "Can not send quit command to phone!");
    return;
  }
  strcpy(command,"quit");
  dict_write_cstring(iter, CMD_KEY, command);
  const uint32_t final_size = dict_write_end(iter);
  app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent command '%s' to phone! (%d bytes)", command, (int) final_size);
  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
