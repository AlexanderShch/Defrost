"""Compile-check the actual firmware/server log layouts against each other."""
import argparse,re,subprocess,tempfile
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('--compiler',required=True);p.add_argument('--server-root',type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[1]
text=(a.server_root/'DataForm.cpp').read_text(encoding='utf-8-sig')
end=text.index('} ControlLogPayload_t;')+len('} ControlLogPayload_t;')
start=text.rfind('typedef struct',0,end)
definition=text[start:end].replace('ControlLogPayload_t','ServerControlLogPayload')
fields=['T_filt_C','phase','eT_common','heatScale01','uCommon_TEN','trim_TEN','uLeft_TEN','uRight_TEN',
        'leftTen1Duty','leftTen2Duty','rightTen1Duty','rightTen2Duty','w_sup_avg','wErr','injDuty',
        'rate_Cps','fishHot_C','fishCold_C','pidIntegral','pidDerivative','pidFeedforward','pidWaterDuty']
code='#include "DefrostControl.h"\n#include "ServerTxSize.h"\n#include <stddef.h>\n#pragma pack(push,1)\n'+definition+'\n#pragma pack(pop)\n'
code+='static_assert(sizeof(ServerControlLogPayload)==105,"server length");\n'
code+='static_assert(sizeof(ControlLogPayload_t)==105,"firmware length");\n'
code+='static_assert(offsetof(ControlLogPayload_t,pidIntegral)==89,"legacy prefix");\n'
for field in fields:
    code+=f'static_assert(offsetof(ServerControlLogPayload,{field})==offsetof(ControlLogPayload_t,{field}),"{field} offset");\n'
code+='static_assert(105+6<=SERVER_TX_ITEM_SIZE-2,"frame fits queue");\n'
with tempfile.TemporaryDirectory(prefix='defrost_log_contract_') as folder:
    source=Path(folder)/'wire.cpp';source.write_text(code,encoding='utf8')
    subprocess.run([a.compiler,'-std=c++11','-fsyntax-only','-I',str(root/'STM32CubeIDE/ProjectCode'),str(source)],check=True)
print('PASS: firmware/server 105-byte layout, all 22 field offsets, legacy 89-byte prefix, TX capacity')
