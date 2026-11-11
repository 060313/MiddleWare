# deviceFile测试用例说明

## S001-初始化测试
- 输入数据：initCase
- 前置条件：无
- 测试步骤：

    1.从initCase读入一条新数据；  
    2.构造deviceFile的桩对象；  
    3.逐一比较桩对象属性与读入数据；  
    4.统计不一致个数并输出；  
    5.若测试集读完，则结束测试，并输出测试结果；否则跳到1。

- 预期输出：

    ...  
    case x:true true true true  
    ...  
    case x:true true true fasle  
    ...   
    ALL:x passed y fatal

## S002-频率表文件解析测试
- 输入数据：freqTableCases  
- 前置条件：无  
- 测试步骤：  
    1. 分别令divisor=0和-1，调用parseFreqTable读入case1，检查输出是否为空表；  
    2. 令divisor=1，调用parseFreqTable读入case0,检查输出是否为空表；
    3. 令divisor=1，分别调用parseFreqTable读入case1到case5并各自检查输出是否为升序正数数数列；
    4. 令divisor=1，调用parseFreqTable读入case6,检查输出是否为空表；
    5. 令divisor=1，分别调用parseFreqTable读入case7到case9并各自检查输出是否为升序正数数数列；
    6. 令divisor=1000，重复第2步到第5步  
- 预期输出：  
    ...  
    exp x : divisor=y casez 函数输出 passeded或fatal  
    ...  
    ALL a passed b fatal
   
## S003-频率表测试
- 输入数据：xpuCases 
- 前置条件：S001、S002
- 测试步骤；  
    1. 实例化一个桩对象sampleA(freqMin_=0,freqMax_=0)，检查freqTable_初始值是否为空；  
    2. 令divisor=1，调用readFreqTable读入case0,检查freqTable_是否为空表；
    3. 令divisor=1，调用readFreqTable读入case1,检查freqTable_是否为文件对应无重复升序序列；
    4. 令divisor=1，调用readFreqTable读入case2,检查freqTable_是否为文件对应无重复升序序列；
    5. 令divisor=1000，调用readFreqTable读入case0,检查freqTable_是否为空表；
    6. 令divisor=1000，调用readFreqTable读入case3,检查freqTable_是否为文件对应无重复升序序列；
    7. 令divisor=1000，调用readFreqTable读入case4,检查freqTable_是否为文件对应无重复升序序列；
    8. 实例化一个桩对象sampleB(freqMin_=minFreq,freqMax_=maxFreq)，检查freqTable_初始值是否为空；  
    9. 对于sampleB，重复第2步到第7步，但检查时应额外检查徐业中每个元素是否都在[minFreq,maxFreq]。

- 预期输出： 
    ...  
    sample Z  
    ...  
    divisor=x casey freqTable_的内容 检查结果  
    ...
    ALL a passed b fatal

