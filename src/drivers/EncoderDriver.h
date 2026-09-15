#pragma once
#include <Arduino.h>
#include <driver/gpio.h>
#include <soc/gpio_struct.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "board/BoardPins.h"
#include "drivers/ButtonGesture.h"
#include "drivers/QuadratureDecoder.h"
namespace aurageek {namespace drivers {
class EncoderDriver {
 public:
  struct Event{int rotation=0;bool click=false,doubleClick=false,back=false,home=false;uint32_t sampledAt=0,maxSampleGap=0,dropped=0,rotationAgeUs=0;};
  void begin(){
    pinMode(board::kEncoderA,INPUT_PULLUP);pinMode(board::kEncoderB,INPUT_PULLUP);
    pinMode(board::kEncoderPush,INPUT_PULLUP);pinMode(board::kAuxButton,INPUT_PULLUP);
    quadrature_.reset(readEncoder());
    attachInterruptArg(board::kEncoderA,onEncoderChange,this,CHANGE);
    attachInterruptArg(board::kEncoderB,onEncoderChange,this,CHANGE);
    Serial.printf("[INPUT] full-cycle A/B decoder; initial_ab=%u; no single-edge direction guesses\n",unsigned(quadrature_.state()));
    buttonQueue_=xQueueCreate(16,sizeof(ButtonMessage));
    if(!buttonQueue_||xTaskCreatePinnedToCore(buttonTask,"buttons",2048,this,3,&buttonTask_,1)!=pdPASS){
      Serial.println("[INPUT] button sampler allocation failed");abort();
    }
    Serial.println("[INPUT] independent 2ms sampler; debounce=20ms; second-down window=300ms");
  }
  Event poll(){
    Event e;
    uint32_t rotationAt=0;
    portENTER_CRITICAL(&rotationMux_);e.rotation=pendingRotation_;rotationAt=pendingRotationAtUs_;pendingRotation_=0;portEXIT_CRITICAL(&rotationMux_);
    if(e.rotation)e.rotationAgeUs=micros()-rotationAt;
    if(e.rotation>4)e.rotation=4;else if(e.rotation<-4)e.rotation=-4;
    ButtonMessage message;
    if(buttonQueue_&&xQueueReceive(buttonQueue_,&message,0)==pdTRUE){
      e.sampledAt=message.at;
      e.click=message.action==1;e.doubleClick=message.action==2;
      e.back=message.action==3;e.home=message.action==4;
    }
    e.maxSampleGap=maxSampleGap_.load();e.dropped=dropped_.load();
    return e;
  }
  void printStatus(){
    uint32_t right,left,invalid;uint8_t decoded;
    portENTER_CRITICAL(&rotationMux_);right=rightSteps_;left=leftSteps_;invalid=quadrature_.invalid();decoded=quadrature_.state();portEXIT_CRITICAL(&rotationMux_);
    Serial.printf("[ENCODER] mode=full-cycle raw_ab=%u decoded_ab=%u right=%lu left=%lu invalid=%lu\n",unsigned(readEncoder()),unsigned(decoded),(unsigned long)right,(unsigned long)left,(unsigned long)invalid);
  }
 private:
  struct ButtonMessage{uint32_t at;uint8_t action;};
  static void buttonTask(void* context){
    auto* self=static_cast<EncoderDriver*>(context);
    ButtonGesture push;ButtonGesture aux(800,0);
    TickType_t wake=xTaskGetTickCount();const TickType_t period=pdMS_TO_TICKS(2)>0?pdMS_TO_TICKS(2):1;
    uint32_t previous=millis();
    for(;;){
      const uint32_t now=millis(),gap=now-previous;previous=now;
      if(gap>self->maxSampleGap_.load())self->maxSampleGap_.store(gap);
      auto a=push.sample(gpio_get_level(static_cast<gpio_num_t>(board::kEncoderPush))==0,now);
      auto b=aux.sample(gpio_get_level(static_cast<gpio_num_t>(board::kAuxButton))==0,now);
      self->enqueue(a,false,now);self->enqueue(b,true,now);
      vTaskDelayUntil(&wake,period);
    }
  }
  void enqueue(ButtonGesture::Action action,bool aux,uint32_t now){
    if(action==ButtonGesture::Action::None)return;
    ButtonMessage message{now,uint8_t(action==ButtonGesture::Action::Long?4:
      (aux?3:(action==ButtonGesture::Action::Double?2:1)))};
    if(xQueueSend(buttonQueue_,&message,0)!=pdTRUE)dropped_.fetch_add(1);
  }
  static uint8_t readEncoder(){
    static_assert(board::kEncoderA>=32&&board::kEncoderB>=32&&board::kEncoderA<54&&board::kEncoderB<54,"Encoder pins must share GPIO input bank 1");
    // One register snapshot: A and B must describe the same instant.
    const uint32_t pins=GPIO.in1.val;
    return uint8_t((((pins>>(board::kEncoderA-32))&1u)<<1)|((pins>>(board::kEncoderB-32))&1u));
  }
  static void onEncoderChange(void* context){
    auto* self=static_cast<EncoderDriver*>(context);const uint32_t now=micros();
    portENTER_CRITICAL_ISR(&self->rotationMux_);
    const int direction=self->quadrature_.sample(readEncoder());
    if(direction){self->pendingRotation_+=direction;self->pendingRotationAtUs_=now;if(direction>0)++self->rightSteps_;else ++self->leftSteps_;}
    portEXIT_CRITICAL_ISR(&self->rotationMux_);
  }
  portMUX_TYPE rotationMux_=portMUX_INITIALIZER_UNLOCKED;
  volatile int pendingRotation_=0;
  volatile uint32_t pendingRotationAtUs_=0;
  QuadratureDecoder quadrature_;
  uint32_t rightSteps_=0,leftSteps_=0;
  QueueHandle_t buttonQueue_=nullptr;
  TaskHandle_t buttonTask_=nullptr;
  std::atomic<uint32_t> maxSampleGap_{0},dropped_{0};
};
}}
