#include <SDL.h>

class fps_timer{
public:
  fps_timer(int fps) :fps(fps) {
    reset();
  }

  void reset(){
    sec_per_frame=1.0/fps;
    cur=SDL_GetTicks();
    timing=0;
  }

  // 時間を消費させる。そもそも時間が間に合っていなかったらfalseを返す。
  // 返り値を見て次のフレームを描画するかどうかきめるとよろしい。
  bool elapse(){
    timing=timing+passage_time()-sec_per_frame;

    if (timing>0){ // 間に合わなかった。
      if (timing>sec_per_frame){ // もう累積がひどい。どうにもならない。
        reset();
        return true; // 強制描画
      }
      return false;
    }
    else{ // 間に合った。残り時間のいくらかを休む。
      SDL_Delay((int)(-timing*1000)); // これでよい?
      return true;
    }
  }

private:
  double passage_time(){
    Uint32 next=SDL_GetTicks();
    double ret;
    if (next<cur)
      ret=(double)(cur+(0xffffffff-next)+1);
    else
      ret=(double)(next-cur);
    cur=next;
    return ret/1000.0;
  }
  
  int fps;
  Uint32 cur;
  double timing;
  double sec_per_frame;
};
