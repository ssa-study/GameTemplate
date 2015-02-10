﻿//
// ストリーミングによる再生
// FIXME:インスタンス生成時に自動的に再生を始める
//

#include "streaming.hpp"
#include <iostream>
#include <thread>
#include <chrono>


// FIXME:読み込みバッファを引数で渡している
void Streaming::queueStream(StreamWav& stream, Source& source, Buffer& buffer,
                            std::vector<char>& sound_buffer) {
  size_t length = stream.read(sound_buffer);
  buffer.bind(stream.isStereo(), &sound_buffer[0], static_cast<u_int>(length), stream.sampleRate());
  source.queueBuffer(buffer);
}
  
// std::threadによる再生
void Streaming::streamProc(const std::string path, const bool loop,
                           std::shared_ptr<Source> source, std::shared_ptr<Param> param) {
  StreamWav stream(path);
  stream.loop(loop);
    
  Buffer buffer[BUFFER_NUM];
  std::vector<char> sound_buffer(BUFFER_SIZE);

  // すべてのストリームバッファを再生キューに積む
  for (u_int i = 0; i < BUFFER_NUM; ++i) {
    queueStream(stream, *source, buffer[i], sound_buffer);
  }

  source->gain(1.0);
  source->play();

  while (!stream.isEnd()) {
    param->mutex.lock();
    if (param->stop) break;
    param->mutex.unlock();
      
    if (source->processed() > 0) {
      ALuint buffer_id = source->unqueueBuffer();

      // FIXME:再生の終わったBufferのidをわざわざ探している
      for (u_int i = 0; i < BUFFER_NUM; ++i) {
        if (buffer_id == buffer[i].id()) {
          // 再生の終わったバッファを再キューイング
          queueStream(stream, *source, buffer[i], sound_buffer);
          break;
        }
      }
    }

    // Don't kill the CPU.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
    
  DOUT << "Finish streaming." << std::endl;
}

  
Streaming::Streaming(const std::string& path, const bool loop) :
  source_(std::make_shared<Source>()),
  param_(std::make_shared<Param>()),
  pause_(false)
{
  DOUT << "Streaming()" << std::endl;

  std::thread thread(streamProc, path, loop, source_, param_);
  thread.detach();
}

  
void Streaming::gain(const float gain) {
  source_->gain(gain);
}

void Streaming::pause(const bool pause) {
  if (param_->stop) return;
    
  pause_ = pause;
  if (pause) {
    source_->pause();
  }
  else {
    source_->play();
  }
}

// 再生を止めて、スレッドを破棄する
void Streaming::stop() {
  gain(0.0);

  std::lock_guard<std::mutex>(param_->mutex);
  param_->stop = true;
}

bool Streaming::isPlaying() {
  if (param_->stop) return false;
    
  return source_->isPlaying();
}
