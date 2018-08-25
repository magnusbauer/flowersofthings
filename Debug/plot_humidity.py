import numpy as np
import matplotlib.pyplot as plt

humidity_data = np.loadtxt("soil_data2.txt")

# print(humidity_data[:,0])

print humidity_data[:,0].shape

print humidity_data

plt.plot(humidity_data[:,0],((humidity_data[:,1]/600)*300)-150,color='green')
plt.plot(humidity_data[:,0],humidity_data[:,2],color='red')
plt.plot(humidity_data[:,0],humidity_data[:,3],color='blue')
plt.show()
