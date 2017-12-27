#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <string.h>


namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    char ch;
    char* t = &ch;
    
    std::ptrdiff_t len =  StackBottom - t;
    delete [](std::get<0>(ctx.Stack));
    
    std::get<0>(ctx.Stack) = new char[len]; 
    std::get<1>(ctx.Stack)  = len;
    
    memcpy(std::get<0>(ctx.Stack), t, len);

}
    

void Engine::Restore(context &ctx) {
    volatile char ch;
    char* t = (char*)&ch;
    
    if((char*)&ch > StackBottom - std::get<1>(ctx.Stack)){
            Restore(ctx); 
        }
    char* top = StackBottom - std::get<1>(ctx.Stack);
    
    memcpy(top, std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
    longjmp(ctx.Environment, 1);

}
    

void Engine::yield() {
    if (alive){
        context * tmp = alive;
        alive -> prev = nullptr;
        alive = alive -> next;
        sched(tmp);
    }

}
    

void Engine::sched(void *routine_) {
    context * ctx = (context*)routine_; 
    ctx->caller = cur_routine;
    
    if (cur_routine != nullptr){ 
        cur_routine->callee = ctx;  
        Store(*cur_routine);
        
        if (setjmp(cur_routine -> Environment) !=0){ 
            return;
        }
    }
    cur_routine = ctx;
    Restore(*ctx);
}

} 
} 
