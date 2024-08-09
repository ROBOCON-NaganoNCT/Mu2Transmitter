#include "buzzer.h"
#include "pin.h"
namespace buzzer{
    //2音同時発生
    Buzzer buzzers[1]={
        Buzzer(BUZZER)
    };
    //2つの楽譜を同時再生
    Player players[2]={
        Player(buzz),
        Player(buzz)
    };
    constexpr uint8_t player_size=sizeof(players)/sizeof(players[0]);
    void init(){
        for(auto &b:buzzers){
            b.init();
        }
    }
    void clear(){
        for(auto &b:buzzers){
            b.clear();
        }
        for(auto &p:players){
            p.stop();
        }
    }
    uint8_t play(Note *score,uint8_t buflength,uint8_t priority,bool autoloop){
        for(int i=0;i<player_size;i++){
            if(players[i].play(score,buflength,priority,autoloop))return i;
        }
        return 255;
    }
    void buzzForce(Note score){
        buzz(score.freq,score.duration,255);
    }
    bool buzz(uint16_t freq,uint16_t duration,uint8_t priority){
        for(auto &b:buzzers){
            if(b.buzz(freq,duration,priority))return true;
        }
        return false;
    }
     bool buzz(Note score ,uint8_t priority){
        for(auto &b:buzzers){
            if(b.buzz(score.freq,score.duration,priority))return true;
        }
        return false;
    }
    void update(){
        for(auto &b:buzzers){
            b.update();
        }
        for(auto &p:players){
            p.update();
        }
    }
    void stopPlayer(uint8_t index){
        if(index>=player_size)return;
        players[index].stop();
    }
    bool is_playing(){
        return false;
    }

};