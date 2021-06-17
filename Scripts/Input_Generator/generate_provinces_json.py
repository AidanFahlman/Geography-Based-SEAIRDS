#!/usr/bin/env python
# coding: utf-8

# In[102]:

# Original author: Kevin
# (Slightly) modified by: Binyamin
# modified further by: Glenn - Feb 18 2021

#!/usr/bin/env python
# coding: utf-8

import pandas as pd
import geopandas as gpd
from collections import defaultdict, OrderedDict
from copy import deepcopy
import json

# data frame index definitions
GDF_REGION_ID = "PRUID"
CLEAN_REGION_ID = "PRcode"
CLEAN_REGION_STR = "PRname"
CLEAN_POP_LABEL = "PRpop_2016"
CLEAN_AREA_LABEL = "PRpop_2016"

# file input output file defitions
OUTPUT_FILE = "output/scenario_ca_provinces.json"
CLEAN_CSV_FILE = "../../cadmium_gis/Canada_Provinces/canada_provinces_clean.csv"
ADJ_FILE = "../../cadmium_gis/Canada_Provinces/canada_province_adjacency.csv"
GDF_FILE = "../../cadmium_gis/Canada_Provinces/canada_provinces.gpkg"

def shared_boundaries(gdf, id1, id2):
    g1 = gdf[gdf[GDF_REGION_ID] == str(id1)].geometry.iloc[0]
    g2 = gdf[gdf[GDF_REGION_ID] == str(id2)].geometry.iloc[0]
    return g1.length, g2.length, g1.boundary.intersection(g2.boundary).length

def get_boundary_length(gdf, id1):
    g1 = gdf[gdf[GDF_REGION_ID] == str(id1)].geometry.iloc[0]
    return g1.boundary.length


# In[103]:


df = pd.read_csv(CLEAN_CSV_FILE)        # General information (id, population, area...)
df_adj = pd.read_csv(ADJ_FILE)          # Pair of adjacent territories
gdf_ontario = gpd.read_file(GDF_FILE)   # GeoDataFrame with the territories poligons


# In[104]:


df.head()


# In[105]:


df_adj.head()


# In[106]:


gdf_ontario.head()


# In[107]:

# read default state from input json
default_cell = json.loads(open("input_provinces/default.json", "r").read())
fields = json.loads(open("input_provinces/fields.json", "r").read())
infectedCell = json.loads(open("input_provinces/infectedCell.json", "r").read())


# In[108]:

default_state = default_cell["default"]["state"]
default_vicinity = default_cell["default"]["neighborhood"]["default_cell_id"]
default_correction_factors = default_vicinity["infection_correction_factors"]
default_correlation = default_vicinity["correlation"]
df_adj.head()


# In[109]:


nan_rows = df[df[CLEAN_POP_LABEL].isnull()]
zero_pop_rows = df[df[CLEAN_POP_LABEL] == 0]
invalid_region_ids = list(pd.concat([nan_rows, zero_pop_rows])[CLEAN_REGION_ID])
len(invalid_region_ids), len(df)


# In[110]:


adj_full = OrderedDict()  # Dictionary with the structure of the json output format

for ind, row in df_adj.iterrows():  # Iterate the different pair of adjacent territories
    if row["region_id"] in invalid_region_ids:
        print("Invalid region_id found: ", row["region_id"])
        continue
    elif row["neighbor_id"] in invalid_region_ids:
        print("Invalid region_id found: ", row["neighbor_id"])
        continue
    elif str(row["region_id"]) not in adj_full:
        rel_row = df[df[CLEAN_REGION_ID] == row["region_id"]].iloc[0, :]
        pop = int(rel_row[CLEAN_POP_LABEL])
        name = rel_row[CLEAN_REGION_STR]
        area = rel_row[CLEAN_AREA_LABEL]

        boundary_len = get_boundary_length(gdf_ontario, row["region_id"])
        
        state = deepcopy(default_state)
        state["population"] = pop
        expr = dict()
        expr[str(row["region_id"])] = {"name": name, "state": state, "neighborhood": {}}
        adj_full[str(row["region_id"])] = expr

    l1, l2, shared = shared_boundaries(gdf_ontario, row["region_id"], row["neighbor_id"])
    correlation = (shared/l1 + shared/l2) / 2  # equation extracted from zhong paper (boundaries only, we don't have roads info for now)
    # correlation = math.e ** (-1/correlation)
    if correlation == 0:
        continue
    key = str(row["region_id"])
    expr = {"correlation": correlation,"infection_correction_factors": default_correction_factors}
    adj_full[key][key]["neighborhood"][str(row["neighbor_id"])]=expr
    
    if ind % 2 == 0:
        print(ind, "%.2f%%" % (100*ind/len(df_adj)))

for key, value in adj_full.items():
    # insert every cell into its own neighborhood, a cell is -> cell = adj_full[key][key]
    adj_full[key][key]["neighborhood"][key] = {"correlation": default_correlation, "infection_correction_factors": default_correction_factors}


# In[111]:

# insert cells from ordered dictionary into index "cells" of a new OrderedDict
template = OrderedDict()
template["cells"] = {}
template["cells"]["default"] = default_cell["default"]
for key, value in adj_full.items():

    # write cells in cadmium master format 
    template["cells"][key] = value[key]

    # overwrite the state variables of the infected cell
    # this should be modified to support any number of infected cells contained in the infectedCell.json file
    if(key == infectedCell["cell_id"]):
        template["cells"][key]["state"]["susceptible"] = infectedCell["state"]["susceptible"]
        template["cells"][key]["state"]["exposed"] = infectedCell["state"]["exposed"]
        template["cells"][key]["state"]["infected"] = infectedCell["state"]["infected"]
        template["cells"][key]["state"]["recovered"] = infectedCell["state"]["recovered"]
        template["cells"][key]["state"]["fatalities"] = infectedCell["state"]["fatalities"]

# insert fields object at the end of the json for use with the GIS Webviewer V2
template["fields"] = fields["fields"]
adj_full_json = json.dumps(template, indent=4, sort_keys=False)  # Dictionary to string (with indentation=4 for better formatting)


# In[112]:


with open(OUTPUT_FILE, "w") as f:
    f.write(adj_full_json)
