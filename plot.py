# importing the required libraries
import matplotlib.pyplot as plt
import numpy as np

def drawall():
    # define data values

    # 0 - 1..16383 - 16384
    x = np.r_[1:16384]  # X-axis points
    #y = x  # Y-axis points

    mode = 0
    match mode:
        case 1:
            y = np.pow(x / 16384, 3)
            y = y[::-1]
        case _:
            y = (-200 * 2 / 960) * np.log(x / 16384) / np.log(10)
            #y = -20.0 / 96.0 * np.log((x * x) / (16384 * 16384)) / np.log(10)

    y = np.clip(y, 0, 1.0)

    x = np.insert(x, 0, 0)
    y = np.insert(y, 0, 1.0)
    x = np.append(x, 16384)
    y = np.append(y, 0.0)

    invconcave = y
    convex = 1 - y
    concave = y[::-1]
    invconvex = 1 - concave

    bix = np.concatenate([ -x[:0:-1], x ])
    biconvex = np.concatenate([ -convex[:0:-1], convex ])
    biconcave = np.concatenate([ -concave[:0:-1], concave ])
    invbiconvex =  - biconvex
    invbiconcave = - biconcave

    fig, axs = plt.subplots(2, 4, figsize=(10,5))

    axs[0, 0].plot(x, concave)  # Plot the chart
    axs[0, 0].set_title("positive concave unipolar")
    axs[0, 0].grid(True)

    axs[0, 1].plot(x, invconcave)  # Plot the chart
    axs[0, 1].set_title("negative concave unipolar")
    axs[0, 1].grid(True)

    axs[0, 2].plot(x, convex)  # Plot the chart
    axs[0, 2].set_title("positive convex unipolar")
    axs[0, 2].grid(True)

    axs[0, 3].plot(x, invconvex)  # Plot the chart
    axs[0, 3].set_title("negative convex unipolar")
    axs[0, 3].grid(True)

    axs[1, 0].plot(bix, biconcave)  # Plot the chart
    axs[1, 0].set_title("positive concave bipolar")
    axs[1, 0].grid(True)

    axs[1, 1].plot(bix, invbiconcave)  # Plot the chart
    axs[1, 1].set_title("negative concave bipolar")
    axs[1, 1].grid(True)

    axs[1, 2].plot(bix, biconvex)  # Plot the chart
    axs[1, 2].set_title("positive convex bipolar")
    axs[1, 2].grid(True)

    axs[1, 3].plot(bix, invbiconvex)  # Plot the chart
    axs[1, 3].set_title("negative convex bipolar")
    axs[1, 3].grid(True)

    plt.show()  # display

def getConvex(x):
    if x <= 0.001:
        y = 0
    elif x >= 0.999:
        y = 1
    else:
        y = 1 - (-200 * 2 / 960) * np.log(x) / np.log(10)
    return y

def getConcave(x):
    # y = (-200 * 2 / 960) * np.log(1 - x) / np.log(10)
    y = 1 - getConvex(1 - x)
    return y

def getBipolarConcave(x):
    if x < 0.5:
        x = (x*2)
        return -getConcave(1-x)
    else:
        x = (x-0.5)*2
        return getConcave(x)

def getBipolarConvex(x):
    if x < 0.5:
        x = (x*2)
        return -getConvex(1-x)
    else:
        x = (x-0.5)*2
        return getConvex(x)

def drawcurve():
    x = np.r_[0:1:0.01]
    positive = lambda i : i
    negative = lambda i : 1 - i
    # neg_i = 1 - i
    y = [getConcave(positive(i)) for i in x]

    plt.plot(x, y)
    plt.grid(True)
    plt.show()

drawcurve()