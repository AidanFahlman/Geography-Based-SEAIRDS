# Original author: Kevin
# Modified by: Binyamin

#!/usr/bin/env python
# coding: utf-8

# In[1]:


import os
import re
from collections import defaultdict
from copy import copy


# In[2]:

#log_file_folder = "/home/eldrazi/CADMIUM/Cadmium-JSON/Geography-Based-SEIIRS/debugLogs/selfGen"

log_file_folder = "../../logs"
log_filename = log_file_folder + "/pandemic_messages.txt"
patt_out_line = "\{(?P<id>.*) ; <(?P<state>[\w,. -]+)>\}"

# state log structure
sIndex = 1
eIndex = 2
iIndex = 3
rIndex = 4
neweIndex = 5
newiIndex = 6
newrIndex = 7
fIndex = 8
asympIndex = 9
asympaccIndex = 10


# In[3]:

COLOR_SUSCEPTIBLE = 'xkcd:blue'
COLOR_INFECTED = 'xkcd:red'
COLOR_EXPOSED = 'xkcd:sienna'
COLOR_RECOVERED = 'xkcd:green'
COLOR_ASYMPTOMATIC = 'xkcd:purple'
COLOR_DEAD = 'xkcd:black'

# In[4]:

# it seems that curr_states is the collection of all cells state information for the current time step
def curr_states_to_df_row(sim_time, curr_states, num_inf, num_expos, num_rec, line_num):
    sus_acc = 0
    expos_acc = 0
    inf_acc = 0
    rec_acc = 0
    dth_acc = 0
    asymp_acc = 0
    
    new_expos = 0
    new_inf = 0
    new_rec = 0

    asymp = 0

    # sum the proportions of sus, expos, inf, rec ... of all cells
    for k in curr_states:
        sus_acc += curr_states[k][sIndex]
        expos_acc += curr_states[k][eIndex]
        inf_acc += curr_states[k][iIndex]
        rec_acc += curr_states[k][rIndex]
        asymp_acc += curr_states[k][asympaccIndex]
        #dth_acc += curr_states[k][num_inf+num_rec+1]

        new_expos += curr_states[k][neweIndex]
        new_inf += curr_states[k][newiIndex]
        new_rec += curr_states[k][newrIndex]
        asymp += curr_states[k][asympIndex]
        current_deaths = curr_states[k][fIndex]
        dth_acc += current_deaths

        #print((curr_states[k][0]), " --- ", (sum(curr_states[k][sIndex:(rIndex+1)])), " >>> ", sus_acc, expos_acc, inf_acc, rec_acc, dth_acc)
        
        # this python assert statement is of the form assert <condition> <error message>
        # I am a little unsure of the sIndex:rIndex
        assert 0.6 <= sum(curr_states[k][sIndex:(rIndex+1)]) + current_deaths < 1.03, (curr_time, sum(curr_states[k][sIndex:(rIndex+1)]), dth_acc, line_num)
        
        # old assert line 
        # assert 0.95 <= sum(curr_states[k][1:4]) + current_deaths < 1.05, (curr_time, sum(curr_states[k][1:4]), dth_acc, line_num)
            
    # then divide by the number of cells to get the numbers as proportion of population in entire cellspace
    num_cells = len(curr_states)
    sus_acc /= num_cells
    expos_acc /= num_cells
    inf_acc /= num_cells
    rec_acc /= num_cells
    dth_acc /= num_cells
    asymp /= num_cells
    asymp_acc /= num_cells

    new_expos /= num_cells
    new_inf /= num_cells
    new_rec /= num_cells
    
    assert 0.98 <= sus_acc + expos_acc + inf_acc + rec_acc + dth_acc + asymp_acc < 1.02, (curr_time, sus_acc, expos_acc, inf_acc, rec_acc, dth_acc, asymp_acc, num_cells)
    
    return [int(sim_time), sus_acc, expos_acc, inf_acc, rec_acc, asymp_acc, new_expos, new_inf, new_rec, asymp, dth_acc]


# In[5]:


states = ["sus", "expos", "infec", "rec", "asymp"]
data = []
curr_data = []
curr_time = None
curr_states = {}
num_inf = 0
num_expos = 0
num_rec = 0
num_asymp = 0

with open(log_filename, "r") as log_file:
    line_num = 0
    
    # for each line, read a line then:
    for line in log_file:
        # strip leading and trailing spaces
        line = line.strip()
        
        # if a time marker is found that is not the current time
        if line.isnumeric() and line != curr_time:
            
            # appears to read the line into the data structure
            if curr_states:
                data.append(curr_states_to_df_row(curr_time, curr_states, num_inf, num_expos, num_rec, line_num))
            curr_time = line
            continue

        match = re.search(patt_out_line, line)
        if not match:
            print(line)
            continue
            
        if not curr_states:
            sp = match.group("state").split(",")

        cid = match.group("id")
        state = list(map(float, match.group("state").split(",")))
        curr_states[cid] = state
        line_num += 1
        
    data.append(curr_states_to_df_row(curr_time, curr_states, num_inf, num_expos, num_rec, line_num))


# In[ ]:

# should be 4 now? I don't know what this line does?
data[:4]

# ### Visualization

# In[ ]:


import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import matplotlib


# In[ ]:


font = {'family' : 'DejaVu Sans',
        'weight' : 'normal',
        'size'   : 16}

matplotlib.rc('font', **font)


# In[ ]:


columns = ["time", "susceptible", "exposed", "infected", "recovered", "asymptomatic", "new_exposed", "new_infected", "new_recovered", "new_asymptomatic", "deaths"]
df_vis = pd.DataFrame(data, columns=columns)
df_vis = df_vis.set_index("time")
df_vis.to_csv("states.csv")
df_vis.head()


# In[ ]:

base_name = log_file_folder + "/pandemic_outputs_"

col_names = ["susceptible", "exposed", "infected", "recovered", "asymptomatic"]
colors=[COLOR_SUSCEPTIBLE, COLOR_EXPOSED, COLOR_INFECTED, COLOR_RECOVERED, COLOR_ASYMPTOMATIC]


# In[ ]:

# draw  lines
fig, ax = plt.subplots(figsize=(15,6))
linewidth = 2

x = list(df_vis.index)
ax.plot(x, df_vis["susceptible"], label="Susceptible", color=COLOR_SUSCEPTIBLE, linewidth=linewidth)
ax.plot(x, df_vis["exposed"], label="Exposed", color=COLOR_EXPOSED, linewidth=linewidth)
ax.plot(x, df_vis["infected"], label="Infected", color=COLOR_INFECTED, linewidth=linewidth)
ax.plot(x, df_vis["recovered"], label="Recovered", color=COLOR_RECOVERED, linewidth=linewidth)
ax.plot(x, df_vis["asymptomatic"], label="Asymptomatic", color=COLOR_ASYMPTOMATIC, linewidth=linewidth)
#ax.plot(x, df_vis["deaths"], label="Deaths", color=COLOR_DEAD, linewidth=linewidth, linestyle=":")
plt.legend(loc='upper right')
plt.margins(0,0)
#plt.title('Epidemic percentages (%s)' % base_name)
plt.xlabel("Time (days)")
plt.ylabel("Population (%)")
#plt.savefig(base_name + "_lines.pdf")
plt.savefig(base_name + "SEIR_lines.png")


# In[ ]:


fig, ax = plt.subplots(figsize=(15,6))
linewidth = 2

x = list(df_vis.index)
ax.plot(x, df_vis["new_exposed"], label="New exposed", color=COLOR_INFECTED, linewidth=linewidth, linestyle=":")
ax.plot(x, df_vis["new_infected"], label="New infected", color=COLOR_INFECTED, linewidth=linewidth, linestyle="--")
ax.plot(x, df_vis["new_recovered"], label="New recovered", color=COLOR_RECOVERED, linewidth=linewidth, linestyle="-.")
ax.plot(x, df_vis["new_asymptomatic"], label="New asymptomatic", color=COLOR_ASYMPTOMATIC, linewidth=linewidth, linestyle="-.")
plt.legend(loc='upper right')
plt.margins(0,0)
#plt.title('Epidemic percentages (%s)' % base_name)
plt.xlabel("Time (days)")
plt.ylabel("Population (%)")
#plt.savefig(base_name + "new_only_lines.pdf")
plt.savefig(base_name + "new_EIR.png")


# In[ ]:


fig, axs = plt.subplots(2, figsize=(15,6))
linewidth = 2

x = list(df_vis.index)
axs[0].plot(x, df_vis["susceptible"], label="Susceptible", color=COLOR_SUSCEPTIBLE, linewidth=linewidth)
axs[0].plot(x, df_vis["exposed"], label="Exposed", color=COLOR_EXPOSED, linewidth=linewidth)
axs[0].plot(x, df_vis["infected"], label="Infected", color=COLOR_INFECTED, linewidth=linewidth)
axs[0].plot(x, df_vis["recovered"], label="Recovered", color=COLOR_RECOVERED, linewidth=linewidth)
axs[0].set_ylabel("Population (%)")
axs[0].legend()

axs[1].plot(x, df_vis["deaths"], label="Deaths", color=COLOR_DEAD, linewidth=linewidth)
axs[1].set_xlabel("Time (days)")
axs[1].set_ylabel("Population (%)")
axs[1].legend()

#plt.margins(0,0)
#plt.title('Epidemic percentages (%s)' % base_name)
#plt.savefig(base_name + "_lines_with_news.pdf")
plt.savefig(base_name + "SEIR_F.png")

#handles, labels = axs[0].get_legend_handles_labels()
#fig.legend(handles, labels, loc=(0.808,0.295))


# In[ ]:




