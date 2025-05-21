import matplotlib.pyplot as plt
import numpy as np  
import os

plt.rcParams['font.family'] = 'Arial'
plt.rcParams['font.size'] = 12  # 你可以根据需要设置其他大小
# define colors 
# 鲜绿色
clrBlue = (0/255,111/255,190/255)
# 深紫色 
clrPurple = (117/255,15/255,109/255)
# 深红色
clrRed = (228/255,46/255,36/255)
# 灰色
clrGrey = (112/255,128/255,144/255)
# 棕色
clrBrown = (188/255,143/255,143/255)
# 橙色
clrOrange = (228/255, 46/255, 36/255)
# 灰蓝色
clrBlueGrey = (112/255, 128/255, 144/255)
# 橄榄色
clrOlive = (188/255, 143/255, 143/255)

fontsizeLable = 16





# 创建图形和轴
fig, ax1 = plt.subplots(figsize=(9, 6))

# 绘制条形图
bar_width = 0.3
x = np.arange(len(labels))
bars1 = ax1.bar(x - bar_width/2, estimated_delays, bar_width, label='Estimated Delay',  color='w', hatch='//', edgecolor=(0/255,111/255,190/255), linewidth=1)
bars2 = ax1.bar(x + bar_width/2, actual_delays, bar_width, label='Actual Delay', color='w', hatch='x', edgecolor=(117/255,15/255,109/255), linewidth=1)

for bar, label in zip(bars1, estimated_delays):
    ax1.text(bar.get_x() + bar.get_width() / 2, bar.get_height(), str(label), ha='center', va='bottom', color=(0/255,111/255,190/255) )
for bar, label in zip(bars2, actual_delays):
    ax1.text(bar.get_x() + bar.get_width() / 2, bar.get_height(), str(label), ha='center', va='bottom', color=(117/255,15/255,109/255) )


ax1.set_xlabel('Circuits', fontsize=16)
ax1.set_ylabel(r'Circuit delay $(ps)$',  fontsize=16)
# ax1.set_title('The substantial discrepancy between the estimated and actual delays')
ax1.set_xticks(x)
ax1.set_xticklabels(labels)
ax1.legend(loc='upper left')



# 创建另一轴用于散点图
ax2 = ax1.twinx()
ax2.set_ylabel( r'$\Delta$Delay (%)', color=(228/255,46/255,36/255), fontsize=16)
ax2.set_ylim(0, 720)
scatters = ax2.scatter(labels, delta_delays, color=(228/255,46/255,36/255), marker='*', label= r'$\Delta$Delay (%)')
# 为散点图添加数值标签
for i, txt in enumerate(delta_delays):
    ax2.annotate(txt, (x[i], delta_delays[i] ), textcoords="offset points", xytext=(0,-13), ha='center', color=(228/255,46/255,36/255))

ax2.legend(loc='upper right', borderpad=0.1, labelspacing=0.1, handletextpad=0.1, borderaxespad=0.5)

plt.gca().tick_params(axis='x', labelsize=14)
plt.gca().tick_params(axis='y', labelsize=14)

# root_path = os.getcwd()  
# save_name = os.path.join(root_path, "motivation_example.pdf")
# plt.savefig(save_name, dpi=300, bbox_inches='tight', transparent=True)

plt.show()
# fig.savefig('D:\AiMap-data\工艺映射-扩展方法\实验结果\motivation_example.pdf', dpi=300, bbox_inches='tight', transparent=True)
# plt.show()
