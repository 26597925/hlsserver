#include <malloc.h>   
#include <stddef.h>

typedef struct tagstStack   
{  
    int udwPc;              /* 栈顶所在位置，指向空闲位置 */  
    int a[STACK_MAX_LEN];  
}stStack;
  
/* 创建新的栈 */  
stStack *stack_new()  
{  
    stStack *p;  
    p = (stStack *)malloc(sizeof(stStack));  
    p->udwPc = 0;  
    return p;  
}  
  
/* 栈复位 */  
void stack_reset(stStack *pstStack)  
{  
    if (NULL == pstStack)  
    {  
        return;  
    }  
    pstStack->udwPc = 0;  
}  
  
/* 入栈，栈满时忽略此次操作 */  
void stack_push(stStack *pstStack, int udwTmp)  
{  
    if (NULL == pstStack)  
    {  
        return;  
    }  
  
    if (STACK_MAX_LEN > pstStack->udwPc)  
    {  
        pstStack->a[pstStack->udwPc] = udwTmp;  
        pstStack->udwPc++;  
    }  
}  
  
/* 出栈，栈空时返回-1 */  
int stack_pop(stStack *pstStack)  
{  
    if (NULL == pstStack)  
    {  
        return -1;  
    }  
  
    if (0 == pstStack->udwPc)  
    {  
        return -1;  
    }  
    return pstStack->a[--pstStack->udwPc];  
}  
  
/* 栈判空 1-空 0-非空 */  
int stack_is_empty(stStack *pstStack)  
{  
    if (NULL == pstStack)  
    {  
        /* 空指针默认为空栈 */  
        return 1;  
    }  
  
    return (0 < pstStack->udwPc)?0:1;  
}  
  
/* 删除栈 */  
void stack_delete(stStack *pstStack)  
{  
    if (NULL != pstStack)  
    {  
        free(pstStack);  
    }  
}  