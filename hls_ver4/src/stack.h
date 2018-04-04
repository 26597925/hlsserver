#include <malloc.h>   
#include <stddef.h>

typedef struct tagstStack   
{  
    int udwPc;              /* ջ������λ�ã�ָ�����λ�� */  
    int a[STACK_MAX_LEN];  
}stStack;
  
/* �����µ�ջ */  
stStack *stack_new()  
{  
    stStack *p;  
    p = (stStack *)malloc(sizeof(stStack));  
    p->udwPc = 0;  
    return p;  
}  
  
/* ջ��λ */  
void stack_reset(stStack *pstStack)  
{  
    if (NULL == pstStack)  
    {  
        return;  
    }  
    pstStack->udwPc = 0;  
}  
  
/* ��ջ��ջ��ʱ���Դ˴β��� */  
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
  
/* ��ջ��ջ��ʱ����-1 */  
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
  
/* ջ�п� 1-�� 0-�ǿ� */  
int stack_is_empty(stStack *pstStack)  
{  
    if (NULL == pstStack)  
    {  
        /* ��ָ��Ĭ��Ϊ��ջ */  
        return 1;  
    }  
  
    return (0 < pstStack->udwPc)?0:1;  
}  
  
/* ɾ��ջ */  
void stack_delete(stStack *pstStack)  
{  
    if (NULL != pstStack)  
    {  
        free(pstStack);  
    }  
}  