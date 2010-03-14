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

  // ���Ԃ��������B�����������Ԃ��Ԃɍ����Ă��Ȃ�������false��Ԃ��B
  // �Ԃ�l�����Ď��̃t���[����`�悷�邩�ǂ������߂�Ƃ�낵���B
  bool elapse(){
    timing=timing+passage_time()-sec_per_frame;

    if (timing>0){ // �Ԃɍ���Ȃ������B
      if (timing>sec_per_frame){ // �����ݐς��Ђǂ��B�ǂ��ɂ��Ȃ�Ȃ��B
        reset();
        return true; // �����`��
      }
      return false;
    }
    else{ // �Ԃɍ������B�c�莞�Ԃ̂����炩���x�ށB
      SDL_Delay((int)(-timing*1000)); // ����ł悢?
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
