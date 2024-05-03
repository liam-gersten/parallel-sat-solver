# inputfile = open('reduced_actual.txt')

# began = False
# output = ""
# for line in inputfile:
#     if line[0] == "I":
#         if began:
#             print(output)
#         began = True
#         output = line[-13:-1] + ", "
#     elif line[:10] == "Solution f":
#         output += line[33:43] + ", " + line[47:52] + ", "
#         i = -8
#         num_str = ""
#         while (line[i] in ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9"]):
#             num_str = line[i] + num_str
#             i -= 1
#         output += num_str + ", "
#     elif line[:10] == "Solution (":
#         output += line[10:16] + ", "
#         num_str = ""
#         i = -2
#         while (line[i] in [".", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"]):
#             num_str = line[i] + num_str
#             i -= 1
#         output += num_str + ", "

# inputfile = open("data.txt")
# prev_n = "n = 1"
# total = 0
# num = 0
# for line in inputfile:
#     data = line.split(', ')
#     if data[4] == prev_n:
#         num += 1
#         total += float(data[5])
#     else:
#         print(prev_n, ": ", total/num)
#         prev_n = data[4]
#         total = 0
#         num = 0
# print(prev_n, ": ", total/num)

inputfile = open("data2.txt")
current_norm = 1
ctr = 0
sums = {2: 0., 4: 0., 8: 0., 16: 0.}
banned = [2.024013754333333, 16.32330399]
banned = [2.024013754333333]
# banned = []
banned_ctr = 0
for line in inputfile:
    data = line.split(' :  ')
    if data[0] == "n = 1":
        current_norm = float(data[1])
        ctr += 1
    else:
        if current_norm in banned:
            banned_ctr += 1
            continue
        n = float(data[0][4:])
        print("Speedup for", data[0], ":", current_norm / float(data[1]))
        sums[n] += current_norm / float(data[1])
for i in sums.keys():
    sums[i] /= (10 - banned_ctr/4)
print(sums)