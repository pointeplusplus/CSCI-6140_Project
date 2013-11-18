import subprocess
import matplotlib.plot as plt


n = "30"
time = "50000000"

ups1 = []
ups2 = []
ups3 = []
ups4 = []
ups_ave = []
uwd = []
uwi = []
te = []

for MPL in range(10,21):
    #output should be in the following format:
    #Ups1 Ups2 Ups3 Ups4 Ups_ave Uwd Uwi te
    output = subprocess.check_output(["./sim", str(MPL), n, time, "arg"])
    output = output.split(" ")
    ups1.append(output[0])
    ups2.append(output[1])
    ups3.append(output[2])
    ups4.append(output[3])
    ups_ave.append(output[4])
    uwd.append(output[5])
    uwi.append(output[6])
    te.append(output[7])

MPL = range(10,21)

plt.figure()
plt.plot(MPL, ups1)
plt.show()

plt.figure()
plt.plot(MPL, ups2)
plt.show()

plt.figure()
plt.plot(MPL, ups3)
plt.show()

plt.figure()
plt.plot(MPL, ups4)
plt.show()

plt.figure()
plt.plot(MPL, ups_ave)
plt.show()

plt.figure()
plt.plot(MPL, uwi)
plt.show()

plt.figure()
plt.plot(MPL, uwd)
plt.show()

plt.figure()
plt.plot(MPL, te)
plt.show()
