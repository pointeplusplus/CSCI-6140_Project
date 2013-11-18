import subprocess
import matplotlib.pyplot as plt


n = "30"
time = "50000"

ups1 = []
ups2 = []
ups3 = []
ups4 = []
ups_ave = []
uwi = []
uwd = []
te = []

for MPL in range(10,21):
    #output should be in the following format:
    #Ups1 Ups2 Ups3 Ups4 Ups_ave Uwi Uwd te
    output = subprocess.check_output(["./sim", str(MPL), n, time, "arg"])
    output = output.split(" ")
    ups1.append(output[0])
    ups2.append(output[1])
    ups3.append(output[2])
    ups4.append(output[3])
    ups_ave.append(output[4])
    uwi.append(output[5])
    uwd.append(output[6])
    te.append(output[7])

MPL = range(10,21)

fig1 = plt.figure()
fig1.suptitle("N = " + n)
plt.title("CPU utilization by user processes")
plt.xlabel("MPL")
plt.ylabel("CPUps")
p1, = plt.plot(MPL, ups1)
p2, = plt.plot(MPL, ups2)
p3, = plt.plot(MPL, ups3)
p4, = plt.plot(MPL, ups4)
p_ave, = plt.plot(MPL, ups_ave)
plt.legend([p1, p2, p3, p4, p_ave], ["Ups1", "Ups2", "Ups3", "Ups4", "Ups_ave"])

fig2 = plt.figure()
fig2.suptitle("N = " + n)
plt.title("CPU utilization while queues empty")
plt.xlabel("MPL")
plt.ylabel("Uwi")
plt.plot(MPL, uwi)

fig3 = plt.figure()
fig3.suptitle("N = " + n)
plt.title("CPU utilization while disk queue non-empty")
plt.xlabel("MPL")
plt.ylabel("Uwd")
plt.plot(MPL, uwd)

fig4 = plt.figure()
fig4.suptitle("N = " + n)
plt.title("Average waiting time in eligible queue")
plt.xlabel("MPL")
plt.ylabel("te")
plt.plot(MPL, te)

plt.show()
