/**
 * @file buzzer.h
 * @author K.Fukushima@nnct (21327@g.nagano-nct.ac.jp)
 * @brief ブザー発声、各基板用定義
 * @version 0.1
 * @date 2024-04-03
 *
 * @copyright Copyright (c) 2024
 *
 */
/*
二つ同時にならせるように制御挟んでます

使い方
buzzer.cppにブザーの定義がすでにあるので、buzzer.hを取り込んで
    void init();//初期化
    void clear();//全てのブザー・再生を停止
    uint8_t play(note *score, uint8_t buflength, uint8_t priority = 0);//楽譜再生
    void buzzForce(note score, uint8_t duration);//強制発声
    bool buzz(uint16_t freq, uint16_t duration, uint8_t priority = 0);//単発発声
    void update();//更新処理、要定期呼び出し
    void stopPlayer(uint8_t index);//再生停止
    bool is_playing();//再生中かどうか

この関数を呼ぶだけ(namespace中のためbuzzer::init()のように呼び出す)

優先度：既に音が鳴っているときに上書きするかどうかの基準（例えば音楽より非常停止音優先するとか）、大きいほど優先

setupでinit呼んで、updateをループでなるべくたくさん(100回/秒ぐらい?)呼んで楽譜、音の長さ処理を自動で行う

単音鳴らしたいならbuzzに周波数長さ優先度を渡すだけ。buzzforceは優先度255で鳴らすだけ

音はnote構造体で定義できて、配列をplay関数に渡せば配列通りに鳴らすこともできる→音楽鳴らせる

stopPlayerで再生中の楽譜を停止できる。引数のindexはplayしたときの返り値なので、再生時するときは返り値を保存して止めたいときにその番号を使うようにする。

is_playingで再生中かどうかを確認できる。trueで再生中

 */
#pragma once
#include <Arduino.h>
#include <math.h>
#include "buzzer_class.h"
namespace buzzer
{
    /**
     * @brief Midiの音階から周波数を計算
     * 
     *
     * @param num 
     * @return freq
     */
    constexpr uint16_t freq(int8_t num)
    {
        return (uint16_t)440 * pow(2, (float)(num - 69) / 12.);
    }

    //ブザー管理インターフェース
    void init();//初期化
    void clear();//全てのブザー・再生を停止
    uint8_t play(Note *score, uint8_t buflength, uint8_t priority = 0,bool autoloop = false);//楽譜再生
    void buzzForce(Note score);//強制発声
    bool buzz(uint16_t freq, uint16_t duration, uint8_t priority = 0);//単発発声
    bool buzz(Note score, uint8_t priority = 0);//単発発声
    void update();//更新処理、要定期呼び出し
    void stopPlayer(uint8_t index);//再生停止
    bool is_playing();//再生中かどうか
};