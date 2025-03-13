import sys
import re
import pandas as pd

# Check if the file path is provided as a command-line argument
if len(sys.argv) < 2:
    print("Usage: python run_abc_extractRes.py <file_path>")
    sys.exit(1)

# The first command-line argument after the script name is the file path
file_path = sys.argv[1]
split_original_res_key = 'statistics'

arrays_arith = ['log2', 'square', 'adder', 'sin', 'div', 'hyp', 'max', 'sqrt', 'multiplier', 'bar']
arrays_control = ['priority', 'cavlc', 'arbiter', 'i2c', 'voter', 'int2float', 'ctrl', 'dec', 'mem_ctrl', 'router']

parseRes = dict()
type = 'arith'
 

def parseOriginalRes(text_data, res:list): 
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
        res.append(
            {
                'design': design_name.group(1),
                '#input': int(i),
                '#output': int(o),
                '#and': int(and_num),
                '#level': int(lev)  
            }
        )
    else:
        print("No match found for original res")

def parseRwRes(text_data, res:list):

    # extract the content before "statistics" onwards
    start_idx = text_data.find(split_original_res_key)
    #extract the desgin name from the content, eg. tv80.aig
    if start_idx == -1:
        print("Split key {} not found".format(split_original_res_key))
        return 
    truncated_text = text_data[start_idx:] 
    design_name = re.search(r'/.*?/([^/]+)\:', truncated_text)
    pattern = r'Resynthesis\s*=\s*([\d.]+)\s*sec.*?Update\s*=\s*([\d.]+)\s*sec.*?TOTAL\s*=\s*([\d.]+)\s*sec.*?i/o\s*=\s*(\d+)/\s*(\d+).*?and\s*=\s*(\d+).*?lev\s*=\s*(\d+)'
    match = re.search(pattern, truncated_text, re.DOTALL)

    if match:
        resyn, update, total, i, o, and_num, lev = match.groups()
        res.append(
            {
                'design': design_name.group(1),
                '#input': int(i),
                '#output': int(o),
                '#and': int(and_num),
                '#level': int(lev),
                'resynthesis': float(resyn),
                'update': float(update),
                'total': float(total)
            }
        )
    else:
        print("No match found for rw res")
  

with open(file_path, 'r') as file:
    # Iterate over each line in the file
    resOriginal = []
    resRw = []
    for line in file: 
       parseOriginalRes(line.strip(), resOriginal)
       parseRwRes(line.strip(), resRw)
    dfOriginal = pd.DataFrame(resOriginal)
    dfOriginal = dfOriginal.sort_values(by='#and', ascending=True)
    
    for index, row in dfOriginal.iterrows():
        print(','.join(map(str, row)))

    dfRw = pd.DataFrame(resRw) 
    dfRw = dfRw.sort_values(by='#and', ascending=True)
    for index, row in dfRw.iterrows():
        print(','.join(map(str, row)))
    
        