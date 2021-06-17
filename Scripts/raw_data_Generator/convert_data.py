# -*- coding: utf-8 -*-
"""
Created on Sun Jan 10 14:07:10 2021

@author: aidan
"""

import pandas
import matplotlib.pyplot as plt

# Import PHU case data
PHU_Case_Deltas = pandas.read_csv("data/PHU/PHU_Case_Deltas.csv")
PHU_Cases_Positives = pandas.read_csv("data/PHU/PHU_Cases_Positives.csv")

# Import provincial case data
provincialDailys = pandas.read_csv("data/provincial/provincialDailys.csv")
provincialDailys1 = provincialDailys[['Abbreviation', 'SummaryDate', 'TotalCases']]

fig, ax = plt.subplots(figsize=(8,6))

for label, df in provincialDailys1.groupby('Abbreviation'):
    df.TotalCases.plot(ax=ax, label=label)
plt.legend()
plt.title('Cumulative cases over time')
plt.savefig('Prov_test_cum.png')

provincialDailys2 = provincialDailys[['Abbreviation', 'SummaryDate', 'DailyTotals']]

fig, ax = plt.subplots(figsize=(8,6))

for label, df in provincialDailys2.groupby('Abbreviation'):
    df.DailyTotals.plot(ax=ax, label=label)
plt.legend()
plt.title('Cumulative cases over time')
plt.savefig('Prov_test_daily.png')


#provincialDailys1.plot(subplots = True, grid=True, title="Sample Data (Unit)",
#    layout=(5, 3), sharex=True, sharey=False, legend=False)
#plt.show()