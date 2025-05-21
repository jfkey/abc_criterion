import sys
import re
import pandas as pd

# Check if the file path is provided as a command-line argument
if len(sys.argv) < 2:
    print("Usage: python extRes.py <file_path>")
    sys.exit(1)

# The first command-line argument after the script name is the file path
file_path = sys.argv[1]


arrays_arith = ['log2', 'square', 'adder', 'sin', 'div', 'hyp', 'max', 'sqrt', 'multiplier', 'bar']
arrays_control = ['priority', 'cavlc', 'arbiter', 'i2c', 'voter', 'int2float', 'ctrl', 'dec', 'mem_ctrl', 'router']

parseRes = dict()
type = 'arith'
 

def parseOriginalRes(text_data): 
    split_original_res_key = '###'
    # extract the content before "statistics" onwards
    start_idx = text_data.find(split_original_res_key)
    #extract the desgin name from the content, eg. tv80.aig
    if start_idx == -1:
        print("Split key {} not found".format(split_original_res_key))
        return 
    truncated_text = text_data[:start_idx] 
     
    design_name = re.search(r'/.*?/([^/]+)\.aig', truncated_text) 
    pattern = r'i/o\s*=\s*(\d+)/\s*(\d+).*?and\s*=\s*(\d+).*?lev\s*=\s*(\d+)'
    match = re.search(pattern, truncated_text, re.DOTALL)

    if match:
        i, o, and_num, lev = match.groups()
        return {
                'design': design_name.group(1),
                '#input': int(i),
                '#output': int(o),
                '#and': int(and_num),
                '#level': int(lev)  
            }
    else:
        print("No match found for original res")

'''
global_time 
global_cut 
global_resynthesis_time 
global_aig_update_time 
global_aig_converter_time 
global_level_updates 
global_reverse_updates 
global_node_rewritten 
and 
lev
'''
def parseRewriteRes(text_data):
    split_original_res_key = '###'
    start_idx = text_data.find(split_original_res_key)
    if start_idx == -1:
        print("Split key {} not found".format(split_original_res_key))
        return 
    truncated_text = text_data[start_idx:]  
    res_dict = {}
     
    # use one dict to store the result
    key = 'global_time'
    pattern = rf"{key}\s*=\s*([\d.]+|\d+)"
    match = re.search(pattern, truncated_text, re.DOTALL) 
    if match:
        res_dict['all_time'] = match.group(1)
        
    
    key = 'global_cut'
    pattern = rf"{key}\s*=\s*([\d.]+|\d+)"
    match = re.search(pattern, truncated_text, re.DOTALL) 
    if match:
        res_dict['cut_time'] = match.group(1)
    
    key = 'global_resynthesis_time'
    pattern = rf"{key}\s*=\s*([\d.]+|\d+)"
    match = re.search(pattern, truncated_text, re.DOTALL) 
    if match:
        res_dict['resyn_time'] = match.group(1)
    
    key = 'global_aig_update_time'
    pattern = rf"{key}\s*=\s*([\d.]+|\d+)"
    match = re.search(pattern, truncated_text, re.DOTALL) 
    if match:
        res_dict['lev_upd_time'] = match.group(1)
    
    key = 'global_aig_converter_time'
    pattern = rf"{key}\s*=\s*([\d.]+|\d+)"
    match = re.search(pattern, truncated_text, re.DOTALL) 
    if match:
        res_dict['aig_conv_time'] = match.group(1)
    
    key = 'global_level_updates'
    pattern = rf"{key}\s*(\d+\.?\d*|\d*\.?\d+)"
    match = re.search(pattern, truncated_text, re.DOTALL) 
    if match:
        res_dict['lev_upd_num'] = int(match.group(1))
    
    key = 'global_reverse_updates'
    pattern = rf"{key}\s*(\d+\.?\d*|\d*\.?\d+)"
    match = re.search(pattern, truncated_text, re.DOTALL) 
    if match:
        res_dict['rev_upd_num'] = int(match.group(1))
    
    key = 'global_node_rewritten'
    pattern = rf"{key}\s*(\d+\.?\d*|\d*\.?\d+)"
    match = re.search(pattern, truncated_text, re.DOTALL) 
    if match:
        res_dict['gain_nodes'] = int(match.group(1))

    key = 'global_reorder_nodes'
    pattern = rf"{key}\s*(\d+\.?\d*|\d*\.?\d+)"
    match = re.search(pattern, truncated_text, re.DOTALL) 
    if match:
        res_dict['reorder_nodes'] = int(match.group(1))
    
    key = 'and'
    pattern = rf"{key}\s*=\s*(\d+)"
    match = re.search(pattern, truncated_text, re.DOTALL) 
    if match:
        res_dict['#and_rw'] = int(match.group(1))
    
    key = 'lev'
    pattern = rf"{key}\s*=\s*(\d+)"
    match = re.search(pattern, truncated_text, re.DOTALL) 
    if match:
        res_dict['#lev_rw'] = match.group(1)

    return res_dict


 
 
with open(file_path, 'r') as file:
    # Iterate over each line in the file
   
    resRewrite = []
    resRefactor = []
    resResub = [] 
    for line in file:  
        original_res = parseOriginalRes(line)
        if "rewrite" in line:

            rewrite_res = parseRewriteRes(line) 
            if original_res and rewrite_res:
                combined_res = {**original_res, **rewrite_res}  # 合并两个字典
                resRewrite.append(combined_res)
            elif original_res:
                resRewrite.append(original_res)  # 如果只有原始数据
            elif rewrite_res:
                resRewrite.append(rewrite_res)  # 如果只有重写数据

        if "refactor" in line:
            refactor_res = parseRewriteRes(line)
            if original_res and refactor_res:
                combined_res = {**original_res, **refactor_res}
                resRefactor.append(combined_res)
            elif original_res:
                resRefactor.append(original_res)
            elif refactor_res:
                resRefactor.append(refactor_res)
        
        if "resub" in line:
            resub_res = parseRewriteRes(line)
            if original_res and resub_res:
                combined_res = {**original_res, **resub_res}
                resResub.append(combined_res)
            elif original_res:
                resResub.append(original_res)
            elif resub_res:
                resResub.append(resub_res)
                
    dfRewrite = pd.DataFrame(resRewrite)
    dfRewrite = dfRewrite.sort_values(by='#and', ascending=True)
    # print(dfRewrite)
    
    dfRefactor = pd.DataFrame(resRefactor)
    dfRefactor = dfRefactor.sort_values(by='#and', ascending=True)
    # print(dfRefactor)
    
    dfResub = pd.DataFrame(resResub)
    dfResub = dfResub.sort_values(by='#and', ascending=True)
    # print(dfResub)
    
    # write to current path/file_name.log
    path = file_path.split('/')[-1]
    dfRewrite.to_csv('./exp-analysis/'+path.split('.')[0]+'_rewrite.csv', index=False)
    dfRefactor.to_csv('./exp-analysis/'+path.split('.')[0]+'_refactor.csv', index=False)
    dfResub.to_csv('./exp-analysis/'+path.split('.')[0]+'_resub.csv', index=False)

     