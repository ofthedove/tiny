/*!
 * @file
 * @brief
 */

#include "CppUTestExt/MockSupport.h"
#include "double/tiny_uart_double.hpp"
#include "tiny_utils.h"

static void send(i_tiny_uart_t* _self, uint8_t byte)
{
  auto self = reinterpret_cast<tiny_uart_double_t*>(_self);
  self->sending = true;
  mock().actualCall("send").onObject(self).withParameter("byte", byte);

  if(self->automatic_send_complete) {
    tiny_uart_double_trigger_send_complete(self);
  }

  if(self->echoing) {
    tiny_uart_double_trigger_receive(self, byte);
  }
}

static i_tiny_event_t* on_send_complete(i_tiny_uart_t* _self)
{
  auto self = reinterpret_cast<tiny_uart_double_t*>(_self);
  return &self->send_complete.interface;
}

static i_tiny_event_t* on_receive(i_tiny_uart_t* _self)
{
  auto self = reinterpret_cast<tiny_uart_double_t*>(_self);
  return &self->receive.interface;
}

static const i_tiny_uart_api_t api = { send, on_send_complete, on_receive };

void tiny_uart_double_init(tiny_uart_double_t* self)
{
  self->interface.api = &api;
  self->sending = false;
  self->automatic_send_complete = false;
  self->echoing = false;

  tiny_event_init(&self->send_complete);
  tiny_event_init(&self->receive);
}

bool tiny_uart_double_sending(tiny_uart_double_t* self)
{
  return self->sending;
}

void tiny_uart_double_trigger_send_complete(tiny_uart_double_t* self)
{
  self->sending = false;
  tiny_event_publish(&self->send_complete, nullptr);
}

void tiny_uart_double_trigger_receive(tiny_uart_double_t* self, uint8_t byte)
{
  tiny_uart_on_receive_args_t args = { byte };
  tiny_event_publish(&self->receive, &args);
}

void tiny_uart_double_configure_automatic_send_complete(tiny_uart_double_t* self, bool enabled)
{
  self->automatic_send_complete = enabled;
}

void tiny_uart_double_enable_echo(tiny_uart_double_t* self)
{
  self->echoing = true;
}
