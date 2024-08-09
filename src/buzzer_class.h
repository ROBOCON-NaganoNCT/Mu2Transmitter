/**
 * @file buzzer.h
 * @author K.Fukushima@nnct (21327@g.nagano-nct.ac.jp)
 * @brief ブザー発声ライブラリ
 * @version 0.1
 * @date 2024-04-03
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once
#include <Arduino.h>
#include <math.h>
    struct Note
    {
        uint16_t freq;
        uint16_t start;
        uint8_t duration;
    };    
    /**
     * @brief ブザー単体制御クラス
     * 
     */
    class Buzzer
    {
    public:
        Buzzer(uint8_t pin) : _pin(pin){};
        void init()
        {
            pinMode(_pin, OUTPUT);
        }
        void clear()
        {
            noTone(_pin);
            _playing = false;
        }
        bool buzz(uint16_t freq, uint16_t duration = 100, uint8_t priority = 0)
        {
            if (is_playing() && _priority > priority&&!_remain_lthan10ms())return false;
            if(freq!=0){
            tone(_pin, freq);
            }
            _start = millis();
            _duration = duration;
            _priority = priority;
            _playing = true;
            return true;
        }
        void update()
        {
            if(!is_playing())return;
            if (millis() - _start >= _duration)
            {
                clear();
            }
        }
        bool is_playing()
        {
            return _playing;
        }

    private:
        //時間の都合で新しい音符と重なって相殺されないように判定をゆるく
        bool _remain_lthan10ms(){
            return millis()-_start>=_duration-10;
        }
        uint8_t _pin;
        uint32_t _start = 0;
        uint16_t _duration = 0;
        uint8_t _priority = 0;
        bool _playing = false;
    };
    /**
     * @brief 楽譜再生クラス　音符の配列を再生
     * 
     */
    class Player
    {
    public:
        Player(bool (*buzz)(uint16_t freq, uint16_t duration, uint8_t priority)) : _buzz(buzz){};
        bool play(Note *score, uint8_t buflength, uint8_t priority = 0,bool autoloop=false)
        {
            if (_playing && _priority >= priority)
                return false;
            _score = score;
            _buflength = buflength;
            _scoreindex = 0;
            _priority = priority;
            _start = millis();
            _playing = true;
            _autoloop = autoloop;
            return true;
        }
        bool stop()
        {
            _playing = false;
            _autoloop = false;
            return true;
        }
        bool update()
        {
            if (!_playing)
                return false;
            while (millis() - _start >= _score[_scoreindex].start * 10)
            {
                _buzz(_score[_scoreindex].freq, _score[_scoreindex].duration * 10, _priority);
                _scoreindex++;
                if (_scoreindex >= _buflength)
                {
                    if(_autoloop){
                        _scoreindex=0;
                        return true;
                    }
                    stop();
                    return false;
                }
            }
            return true;
        }
    private:
        Note* _score;
        uint8_t _buflength;
        uint8_t _scoreindex;
        uint8_t _priority;
        uint32_t _start;
        bool _autoloop=false;
        bool _playing = false;
        bool (*_buzz)(uint16_t freq, uint16_t duration, uint8_t priority);
    };
