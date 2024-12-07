/*!
 * @file
 * @brief
 */

#include "tiny_event_queue.h"
#include "tiny_stack_allocator.h"
#include "tiny_utils.h"

enum {
  type_event,
  type_event_with_data,

  event_overhead =
    sizeof(uint8_t) + // start
    sizeof(tiny_event_queue_callback_t), // callback

  event_with_data_overhead =
    sizeof(uint8_t) + // start
    sizeof(tiny_event_queue_callback_with_data_t) + // callback
    sizeof(uint8_t) // data size
};

static const uint8_t event_type_value = type_event;
static const uint8_t event_type_with_data_value = type_event_with_data;

static void write_to_buffer(tiny_event_queue_t* self, const void* _data, uint8_t data_size)
{
  const uint8_t* data = _data;

  for(uint16_t i = 0; i < data_size; i++) {
    tiny_ring_buffer_insert(&self->ring_buffer, data++);
  }
}

static void read_from_buffer(tiny_event_queue_t* self, void* _data, uint8_t data_size)
{
  uint8_t* data = _data;

  for(uint16_t i = 0; i < data_size; i++) {
    tiny_ring_buffer_remove(&self->ring_buffer, data++);
  }
}

static void enqueue(
  i_tiny_event_queue_t* _self,
  tiny_event_queue_callback_t callback)
{
  reinterpret(self, _self, tiny_event_queue_t*);
  unsigned capacity = tiny_ring_buffer_capacity(&self->ring_buffer);
  unsigned count = tiny_ring_buffer_count(&self->ring_buffer);
  unsigned event_size = event_overhead;

  if((capacity - count) <= event_size) {
    self->unable_to_queue_callback();
    return;
  }

  write_to_buffer(self, &event_type_value, sizeof(event_type_value));
  write_to_buffer(self, &callback, sizeof(callback));
}

static void enqueue_with_data(
  i_tiny_event_queue_t* _self,
  tiny_event_queue_callback_with_data_t callback,
  const void* data,
  uint8_t data_size)
{
  reinterpret(self, _self, tiny_event_queue_t*);
  unsigned capacity = tiny_ring_buffer_capacity(&self->ring_buffer);
  unsigned count = tiny_ring_buffer_count(&self->ring_buffer);
  unsigned event_size = (unsigned)(event_with_data_overhead + data_size);

  if((capacity - count) <= event_size) {
    self->unable_to_queue_callback();
    return;
  }

  write_to_buffer(self, &event_type_with_data_value, sizeof(event_type_with_data_value));
  write_to_buffer(self, &callback, sizeof(callback));
  write_to_buffer(self, &data_size, sizeof(data_size));
  write_to_buffer(self, data, data_size);
}

static const i_tiny_event_queue_api_t api = { enqueue, enqueue_with_data };

void tiny_event_queue_init(
  tiny_event_queue_t* self,
  void* buffer,
  unsigned buffer_size,
  tiny_queue_unable_to_enqueue_callback_t unable_to_queue_callback)
{
  self->interface.api = &api;
  self->unable_to_queue_callback = unable_to_queue_callback;
  tiny_ring_buffer_init(&self->ring_buffer, buffer, 1, buffer_size);
}

static void process_event(tiny_event_queue_t* self)
{
  tiny_event_queue_callback_t callback;
  read_from_buffer(self, &callback, sizeof(callback));
  callback();
}

typedef struct {
  tiny_event_queue_t* self;
  tiny_event_queue_callback_with_data_t callback;
  uint8_t data_size;
} process_event_with_data_context_t;

static void process_event_with_data_worker(void* _context, void* data)
{
  process_event_with_data_context_t* context = _context;
  read_from_buffer(context->self, data, context->data_size);
  context->callback(data);
}

static void process_event_with_data(tiny_event_queue_t* self)
{
  process_event_with_data_context_t context = { .self = self };

  read_from_buffer(self, &context.callback, sizeof(context.callback));
  read_from_buffer(self, &context.data_size, sizeof(context.data_size));

  tiny_stack_allocator_allocate_aligned(
    context.data_size,
    &context,
    process_event_with_data_worker);
}

bool tiny_event_queue_run(tiny_event_queue_t* self)
{
  if(tiny_ring_buffer_count(&self->ring_buffer) == 0) {
    return false;
  }

  uint8_t type;
  read_from_buffer(self, &type, sizeof(type));

  switch(type) {
    case type_event:
      process_event(self);
      break;

    case type_event_with_data:
      process_event_with_data(self);
      break;
  }

  return true;
}
