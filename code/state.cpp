#include "state.h"
#include <list>
#include <cassert> 
#include <unistd.h>

#include <map>

State::State(
        short pid,
        short nprocs,
        short branching_factor,
        short assignment_method,
        bool use_smart_prop)
    {
    State::pid = pid;
    State::nprocs = nprocs;
    State::parent_id = (pid - 1) / branching_factor;
    State::num_children = branching_factor;
    State::branching_factor = branching_factor;
    short actual_num_children = 0;
    for (short child = 0; child < branching_factor; child++) {
        short child_pid = pid_from_child_index(child);
        if (child_pid >= State::nprocs) {
            break;
        }
        actual_num_children++;
    }
    State::num_children = actual_num_children;
    if (PRINT_LEVEL > 0) printf("PID %d: has %d children\n", pid, (int)State::num_children);
    State::child_statuses = (char *)malloc(
        sizeof(char) * (State::num_children + 1));
    for (short child = 0; child <= State::num_children; child++) {
        State::child_statuses[child] = 'w';
    }
    State::requests_sent = (char *)calloc(
        sizeof(char), State::num_children + 1);
    for (short child = 0; child < State::num_children; child++) {
        State::requests_sent[child] = 'n';
    }
    if (pid == 0) {
        // Don't want to request the parent that doesn't exist
        State::requests_sent[State::num_children] = 'u';
    } else {
        State::requests_sent[State::num_children] = 'n';
    }
    State::num_urgent = 0;
    State::num_requesting = 0;
    State::num_non_trivial_tasks = 0;
    State::process_finished = false;
    State::was_explicit_abort = false;
    State::calls_to_solve = 0;
    State::assignment_method = assignment_method;
    State::use_smart_prop = use_smart_prop;
    State::current_cycle = 0;
    Deque thieves;
    State::thieves = (Deque *)malloc(sizeof(Deque));
    *State::thieves = thieves;
    GivenTask current_task;
    current_task.pid = -1;
    current_task.var_id = -1;
    State::current_task = current_task;

    int local_ans[2240] = {13, 16, 39, 56, 74, 95, 97, 116, 134, 156, 169, 178, 195, 213, 235, 254, 271, 275, 298, 310, 331, 340, 366, 369, 397, 407, 418, 440, 453, 476, 489, 496, 521, 542, 550, 560, 584, 605, 612, 626, 645, 671, 675, 698, 716, 731, 737, 759, 775, 794, 801, 828, 836, 848, 877, 889, 904, 915, 942, 949, 971, 982, 994, 1023, 1028, 1051, 1059, 1085, 1088, 1109, 1128, 1151, 1162, 1169, 1196, 1206, 1225, 1246, 1255, 1266, 1288, 1301, 1312, 1342, 1351, 1370, 1379, 1405, 1410, 1428, 1455, 1465, 1478, 1489, 1516, 1531, 1548, 1554, 1572, 1595, 1601, 1624, 1639, 1662, 1664, 1689, 1701, 1727, 1741, 1754, 1763, 1782, 1797, 1823, 1835, 1843, 1858, 1884, 1897, 1911, 1921, 1942, 1962, 1982, 1992, 2013, 2016, 2036, 2051, 2076, 2089, 2100, 2125, 2130, 2154, 2166, 2187, 2206, 2215, 2224, 2241, 2264, 2287, 2293, 2315, 2333, 2338, 2359, 2377, 2390, 2400, 2428, 2446, 2456, 2465, 2484, 2506, 2527, 2533, 2547, 2574, 2585, 2597, 2610, 2627, 2647, 2668, 2680, 2703, 2715, 2726, 2737, 2756, 2768, 2797, 2810, 2818, 2839, 2861, 2873, 2886, 2899, 2927, 2938, 2956, 2965, 2976, 3003, 3022, 3028, 3048, 3057, 3072, 3094, 3119, 3130, 3148, 3153, 3179, 3189, 3209, 3229, 3236, 3251, 3271, 3282, 3310, 3320, 3338, 3345, 3372, 3381, 3406, 3419, 3430, 3440, 3463, 3474, 3496, 3517, 3535, 3539, 3556, 3577, 3585, 3604, 3624, 3647, 3653, 3678, 3682, 3707, 3715, 3738, 3757, 3767, 3776, 3801, 3814, 3836, 3846, 3864, 3886, 3889, 3919, 3929, 3941, 3955, 3972, 3984, 4011, 4028, 4034, 4055, 4074, 4093, 4102, 4103, 4108, 4109, 4114, 4115, 4120, 4121, 4129, 4142, 4143, 4151, 4154, 4155, 4162, 4163, 4168, 4169, 4180, 4181, 4188, 4189, 4197, 4203, 4208, 4209, 4222, 4223, 4230, 4231, 4236, 4237, 4246, 4247, 4250, 4251, 4256, 4257, 4267, 4274, 4275, 4287, 4292, 4293, 4302, 4303, 4304, 4305, 4316, 4317, 4322, 4323, 4328, 4329, 4341, 4345, 4356, 4357, 4365, 4370, 4371, 4383, 4390, 4391, 4398, 4399, 4406, 4407, 4412, 4413, 4418, 4419, 4427, 4432, 4433, 4440, 4441, 4449, 4456, 4457, 4468, 4469, 4474, 4475, 4482, 4483, 4491, 4501, 4508, 4509, 4516, 4517, 4521, 4528, 4529, 4538, 4539, 4544, 4545, 4558, 4559, 4566, 4567, 4568, 4569, 4583, 4588, 4589, 4594, 4595, 4606, 4607, 4611, 4617, 4628, 4629, 4636, 4637, 4644, 4645, 4650, 4651, 4656, 4657, 4670, 4671, 4672, 4673, 4685, 4695, 4698, 4699, 4710, 4711, 4718, 4719, 4722, 4723, 4728, 4729, 4741, 4746, 4747, 4752, 4753, 4762, 4763, 4770, 4771, 4780, 4781, 4787, 4792, 4793, 4801, 4808, 4809, 4822, 4823, 4831, 4838, 4839, 4846, 4847, 4852, 4853, 4860, 4861, 4871, 4872, 4873, 4881, 4894, 4895, 4896, 4897, 4909, 4916, 4917, 4924, 4925, 4931, 4938, 4939, 4946, 4947, 4952, 4953, 4964, 4965, 4974, 4975, 4982, 4983, 4986, 4987, 4994, 4995, 5000, 5001, 5008, 5009, 5021, 5030, 5031, 5039, 5042, 5043, 5054, 5055, 5060, 5061, 5068, 5069, 5076, 5077, 5081, 5088, 5089, 5098, 5099, 5110, 5111, 5115, 5120, 5121, 5134, 5135, 5140, 5141, 5144, 5145, 5159, 5160, 5161, 5174, 5175, 5181, 5188, 5189, 5196, 5197, 5202, 5203, 5214, 5215, 5219, 5226, 5227, 5233, 5242, 5243, 5252, 5253, 5260, 5261, 5270, 5271, 5276, 5277, 5286, 5287, 5290, 5291, 5301, 5310, 5311, 5312, 5313, 5321, 5328, 5329, 5338, 5339, 5346, 5347, 5359, 5363, 5368, 5369, 5382, 5383, 5384, 5385, 5398, 5399, 5403, 5412, 5413, 5418, 5419, 5428, 5429, 5433, 5442, 5443, 5455, 5458, 5459, 5470, 5471, 5472, 5473, 5485, 5488, 5489, 5500, 5501, 5504, 5505, 5514, 5515, 5527, 5534, 5535, 5538, 5539, 5544, 5545, 5554, 5555, 5563, 5574, 5575, 5582, 5583, 5585, 5597, 5604, 5605, 5612, 5613, 5620, 5621, 5624, 5625, 5633, 5647, 5652, 5653, 5658, 5659, 5666, 5667, 5672, 5673, 5686, 5687, 5688, 5689, 5702, 5703, 5708, 5709, 5715, 5726, 5727, 5732, 5733, 5738, 5739, 5744, 5745, 5757, 5764, 5765, 5774, 5775, 5778, 5779, 5790, 5791, 5792, 5793, 5804, 5805, 5808, 5809, 5816, 5817, 5831, 5834, 5835, 5845, 5851, 5858, 5859, 5865, 5878, 5879, 5884, 5885, 5888, 5889, 5898, 5899, 5904, 5905, 5913, 5923, 5934, 5935, 5940, 5941, 5948, 5949, 5957, 5960, 5961, 5974, 5975, 5978, 5979, 5990, 5991, 5996, 5997, 6002, 6003, 6015, 6018, 6019, 6030, 6031, 6035, 6040, 6041, 6053, 6060, 6061, 6065, 6074, 6075, 6086, 6087, 6090, 6091, 6100, 6101, 6108, 6109, 6112, 6113, 6120, 6121, 6135, 6142, 6143, 6145, 6146, 6147, 6148, 6149, 6160, 6161, 6173, 6179, 6182, 6183, 6184, 6185, 6186, 6187, 6188, 6189, 6190, 6191, 6196, 6197, 6198, 6199, 6200, 6201, 6202, 6203, 6205, 6206, 6207, 6208, 6209, 6213, 6214, 6215, 6216, 6217, 6218, 6219, 6220, 6221, 6224, 6225, 6226, 6227, 6231, 6232, 6233, 6240, 6241, 6242, 6243, 6244, 6245, 6246, 6247, 6248, 6249, 6250, 6251, 6257, 6258, 6259, 6260, 6261, 6262, 6263, 6268, 6269, 6271, 6272, 6273, 6274, 6275, 6291, 6292, 6293, 6299, 6302, 6303, 6304, 6305, 6308, 6309, 6310, 6311, 6315, 6316, 6317, 6322, 6323, 6331, 6332, 6333, 6334, 6335, 6337, 6338, 6339, 6340, 6341, 6346, 6347, 6350, 6351, 6352, 6353, 6356, 6357, 6358, 6359, 6363, 6364, 6365, 6369, 6370, 6371, 6372, 6373, 6374, 6375, 6376, 6377, 6388, 6389, 6390, 6391, 6392, 6393, 6394, 6395, 6408, 6409, 6410, 6411, 6412, 6413, 6415, 6416, 6417, 6418, 6419, 6425, 6431, 6435, 6436, 6437, 6449, 6453, 6454, 6455, 6458, 6459, 6460, 6461, 6463, 6464, 6465, 6466, 6467, 6472, 6473, 6474, 6475, 6476, 6477, 6478, 6479, 6480, 6481, 6482, 6483, 6484, 6485, 6491, 6499, 6500, 6501, 6502, 6503, 6510, 6511, 6512, 6513, 6514, 6515, 6520, 6521, 6524, 6525, 6526, 6527, 6533, 6534, 6535, 6536, 6537, 6538, 6539, 6544, 6545, 6549, 6550, 6551, 6557, 6576, 6577, 6578, 6579, 6580, 6581, 6586, 6587, 6589, 6590, 6591, 6592, 6593, 6596, 6597, 6598, 6599, 6602, 6603, 6604, 6605, 6607, 6608, 6609, 6610, 6611, 6612, 6613, 6614, 6615, 6616, 6617, 6621, 6622, 6623, 6627, 6628, 6629, 6634, 6635, 6638, 6639, 6640, 6641, 6642, 6643, 6644, 6645, 6646, 6647, 6648, 6649, 6650, 6651, 6652, 6653, 6659, 6660, 6661, 6662, 6663, 6664, 6665, 6667, 6668, 6669, 6670, 6671, 6679, 6680, 6681, 6682, 6683, 6688, 6689, 6695, 6710, 6711, 6712, 6713, 6717, 6718, 6719, 6720, 6721, 6722, 6723, 6724, 6725, 6727, 6728, 6729, 6730, 6731, 6745, 6746, 6747, 6748, 6749, 6752, 6753, 6754, 6755, 6756, 6757, 6758, 6759, 6760, 6761, 6765, 6766, 6767, 6777, 6778, 6779, 6782, 6783, 6784, 6785, 6791, 6796, 6797, 6803, 6804, 6805, 6806, 6807, 6808, 6809, 6814, 6815, 6821, 6823, 6824, 6825, 6826, 6827, 6836, 6837, 6838, 6839, 6844, 6845, 6848, 6849, 6850, 6851, 6856, 6857, 6861, 6862, 6863, 6864, 6865, 6866, 6867, 6868, 6869, 6879, 6880, 6881, 6882, 6883, 6884, 6885, 6886, 6887, 6889, 6890, 6891, 6892, 6893, 6894, 6895, 6896, 6897, 6898, 6899, 6905, 6916, 6917, 6918, 6919, 6920, 6921, 6922, 6923, 6924, 6925, 6926, 6927, 6928, 6929, 6946, 6947, 6953, 6956, 6957, 6958, 6959, 6965, 6968, 6969, 6970, 6971, 6972, 6973, 6974, 6975, 6976, 6977, 6987, 6988, 6989, 6993, 6994, 6995, 6997, 6998, 6999, 7000, 7001, 7003, 7004, 7005, 7006, 7007, 7016, 7017, 7018, 7019, 7023, 7024, 7025, 7027, 7028, 7029, 7030, 7031, 7034, 7035, 7036, 7037, 7038, 7039, 7040, 7041, 7042, 7043, 7047, 7048, 7049, 7055, 7057, 7058, 7059, 7060, 7061, 7066, 7067, 7078, 7079, 7085, 7086, 7087, 7088, 7089, 7090, 7091, 7098, 7099, 7100, 7101, 7102, 7103, 7104, 7105, 7106, 7107, 7108, 7109, 7115, 7123, 7124, 7125, 7126, 7127, 7138, 7139, 7143, 7144, 7145, 7147, 7148, 7149, 7150, 7151, 7157, 7158, 7159, 7160, 7161, 7162, 7163, 7167, 7168, 7169, 7178, 7179, 7180, 7181, 7186, 7187, 7188, 7189, 7190, 7191, 7192, 7193, 7196, 7197, 7198, 7199, 7202, 7203, 7204, 7205, 7211, 7215, 7216, 7217, 7222, 7223, 7230, 7231, 7232, 7233, 7234, 7235, 7237, 7238, 7239, 7240, 7241, 7242, 7243, 7244, 7245, 7246, 7247, 7251, 7252, 7253, 7256, 7257, 7258, 7259, 7260, 7261, 7262, 7263, 7264, 7265, 7267, 7268, 7269, 7270, 7271, 7277, 7294, 7295, 7300, 7301, 7305, 7306, 7307, 7308, 7309, 7310, 7311, 7312, 7313, 7319, 7321, 7322, 7323, 7324, 7325, 7329, 7330, 7331, 7334, 7335, 7336, 7337, 7340, 7341, 7342, 7343, 7345, 7346, 7347, 7348, 7349, 7361, 7362, 7363, 7364, 7365, 7366, 7367, 7384, 7385, 7386, 7387, 7388, 7389, 7390, 7391, 7400, 7401, 7402, 7403, 7405, 7406, 7407, 7408, 7409, 7415, 7419, 7420, 7421, 7433, 7438, 7439, 7447, 7448, 7449, 7450, 7451, 7452, 7453, 7454, 7455, 7456, 7457, 7458, 7459, 7460, 7461, 7462, 7463, 7468, 7469, 7473, 7474, 7475, 7478, 7479, 7480, 7481, 7482, 7483, 7484, 7485, 7486, 7487, 7490, 7491, 7492, 7493, 7500, 7501, 7502, 7503, 7504, 7505, 7510, 7511, 7512, 7513, 7514, 7515, 7516, 7517, 7518, 7519, 7520, 7521, 7522, 7523, 7534, 7535, 7538, 7539, 7540, 7541, 7545, 7546, 7547, 7553, 7557, 7558, 7559, 7561, 7562, 7563, 7564, 7565, 7571, 7573, 7574, 7575, 7576, 7577, 7593, 7594, 7595, 7597, 7598, 7599, 7600, 7601, 7602, 7603, 7604, 7605, 7606, 7607, 7608, 7609, 7610, 7611, 7612, 7613, 7621, 7622, 7623, 7624, 7625, 7631, 7634, 7635, 7636, 7637, 7648, 7649, 7654, 7655, 7656, 7657, 7658, 7659, 7660, 7661, 7664, 7665, 7666, 7667, 7671, 7672, 7673, 7679, 7690, 7691, 7693, 7694, 7695, 7696, 7697, 7699, 7700, 7701, 7702, 7703, 7707, 7708, 7709, 7710, 7711, 7712, 7713, 7714, 7715, 7719, 7720, 7721, 7733, 7736, 7737, 7738, 7739, 7745, 7746, 7747, 7748, 7749, 7750, 7751, 7754, 7755, 7756, 7757, 7764, 7765, 7766, 7767, 7768, 7769, 7774, 7775, 7776, 7777, 7778, 7779, 7780, 7781, 7785, 7786, 7787, 7790, 7791, 7792, 7793, 7804, 7805, 7807, 7808, 7809, 7810, 7811, 7817, 7820, 7821, 7822, 7823, 7825, 7826, 7827, 7828, 7829, 7830, 7831, 7832, 7833, 7834, 7835, 7847, 7848, 7849, 7850, 7851, 7852, 7853, 7857, 7858, 7859, 7864, 7865, 7877, 7878, 7879, 7880, 7881, 7882, 7883, 7887, 7888, 7889, 7892, 7893, 7894, 7895, 7900, 7901, 7905, 7906, 7907, 7914, 7915, 7916, 7917, 7918, 7919, 7925, 7933, 7934, 7935, 7936, 7937, 7938, 7939, 7940, 7941, 7942, 7943, 7945, 7946, 7947, 7948, 7949, 7954, 7955, 7964, 7965, 7966, 7967, 7972, 7973, 7980, 7981, 7982, 7983, 7984, 7985, 7988, 7989, 7990, 7991, 7997, 7999, 8000, 8001, 8002, 8003, 8004, 8005, 8006, 8007, 8008, 8009, 8019, 8020, 8021, 8025, 8026, 8027, 8035, 8036, 8037, 8038, 8039, 8045, 8050, 8051, 8054, 8055, 8056, 8057, 8058, 8059, 8060, 8061, 8062, 8063, 8069, 8072, 8073, 8074, 8075, 8076, 8077, 8078, 8079, 8080, 8081, 8098, 8099, 8101, 8102, 8103, 8104, 8105, 8106, 8107, 8108, 8109, 8110, 8111, 8116, 8117, 8119, 8120, 8121, 8122, 8123, 8127, 8128, 8129, 8138, 8139, 8140, 8141, 8147, 8148, 8149, 8150, 8151, 8152, 8153, 8157, 8158, 8159, 8166, 8167, 8168, 8169, 8170, 8171, 8175, 8176, 8177, 8182, 8183, 8186, 8187, 8188, 8189, 8197, 8198, 8199, 8200, 8201, 8207, 8215, 8216, 8217, 8218, 8219, 8224, 8225, 8226, 8227, 8228, 8229, 8230, 8231, 8232, 8233, 8234, 8235, 8236, 8237, 8241, 8242, 8243, 8249, 8252, 8253, 8254, 8255, 8259, 8260, 8261, 8267, 8274, 8275, 8276, 8277, 8278, 8279, 8288, 8289, 8290, 8291, 8292, 8293, 8294, 8295, 8296, 8297, 8302, 8303, 8308, 8309, 8312, 8313, 8314, 8315, 8316, 8317, 8318, 8319, 8320, 8321, 8327, 8335, 8336, 8337, 8338, 8339, 8343, 8344, 8345, 8347, 8348, 8349, 8350, 8351, 8352, 8353, 8354, 8355, 8356, 8357, 8361, 8362, 8363, 8369, 8375, 8376, 8377, 8378, 8379, 8380, 8381, 8390, 8391, 8392, 8393, 8397, 8398, 8399, 8401, 8402, 8403, 8404, 8405, 8410, 8411, 8414, 8415, 8416, 8417, 8428, 8429, 8430, 8431, 8432, 8433, 8434, 8435, 8437, 8438, 8439, 8440, 8441, 8449, 8450, 8451, 8452, 8453, 8455, 8456, 8457, 8458, 8459, 8464, 8465, 8477, 8478, 8479, 8480, 8481, 8482, 8483, 8487, 8488, 8489, 8492, 8493, 8494, 8495, 8496, 8497, 8498, 8499, 8500, 8501, 8512, 8513, 8517, 8518, 8519, 8531, 8534, 8535, 8536, 8537, 8538, 8539, 8540, 8541, 8542, 8543, 8548, 8549, 8552, 8553, 8554, 8555, 8562, 8563, 8564, 8565, 8566, 8567, 8569, 8570, 8571, 8572, 8573, 8574, 8575, 8576, 8577, 8578, 8579, 8584, 8585, 8591, 8594, 8595, 8596, 8597, 8603, 8605, 8606, 8607, 8608, 8609, 8619, 8620, 8621, 8631, 8632, 8633, 8634, 8635, 8636, 8637, 8638, 8639, 8640, 8641, 8642, 8643, 8644, 8645, 8647, 8648, 8649, 8650, 8651, 8657, 8663, 8665, 8666, 8667, 8668, 8669, 8678, 8679, 8680, 8681, 8686, 8687, 8691, 8692, 8693, 8700, 8701, 8702, 8703, 8704, 8705, 8708, 8709, 8710, 8711, 8716, 8717, 8718, 8719, 8720, 8721, 8722, 8723, 8733, 8734, 8735, 8738, 8739, 8740, 8741, 8742, 8743, 8744, 8745, 8746, 8747, 8758, 8759, 8760, 8761, 8762, 8763, 8764, 8765, 8768, 8769, 8770, 8771, 8777, 8784, 8785, 8786, 8787, 8788, 8789, 8795, 8799, 8800, 8801, 8803, 8804, 8805, 8806, 8807, 8811, 8812, 8813, 8821, 8822, 8823, 8824, 8825, 8830, 8831, 8842, 8843, 8844, 8845, 8846, 8847, 8848, 8849, 8851, 8852, 8853, 8854, 8855, 8858, 8859, 8860, 8861, 8865, 8866, 8867, 8875, 8876, 8877, 8878, 8879, 8880, 8881, 8882, 8883, 8884, 8885, 8889, 8890, 8891, 8897, 8902, 8903, 8909, 8912, 8913, 8914, 8915, 8916, 8917, 8918, 8919, 8920, 8921, 8931, 8932, 8933, 8942, 8943, 8944, 8945, 8946, 8947, 8948, 8949, 8950, 8951, 8952, 8953, 8954, 8955, 8956, 8957, 8963, 8968, 8969, 8973, 8974, 8975, 8978, 8979, 8980, 8981, 8982, 8983, 8984, 8985, 8986, 8987, 8998, 8999, 9007, 9008, 9009, 9010, 9011, 9017, 9019, 9020, 9021, 9022, 9023, 9025, 9026, 9027, 9028, 9029, 9040, 9041, 9045, 9046, 9047, 9059, 9060, 9061, 9062, 9063, 9064, 9065, 9067, 9068, 9069, 9070, 9071, 9078, 9079, 9080, 9081, 9082, 9083, 9086, 9087, 9088, 9089, 9093, 9094, 9095, 9096, 9097, 9098, 9099, 9100, 9101, 9104, 9105, 9106, 9107, 9112, 9113, 9119, 9122, 9123, 9124, 9125, 9131, 9133, 9134, 9135, 9136, 9137, 9141, 9142, 9143, 9147, 9148, 9149, 9154, 9155, 9162, 9163, 9164, 9165, 9166, 9167, 9178, 9179, 9180, 9181, 9182, 9183, 9184, 9185, 9188, 9189, 9190, 9191, 9193, 9194, 9195, 9196, 9197, 9198, 9199, 9200, 9201, 9202, 9203, 9215};
    State::ans = (bool*) malloc(sizeof(bool)*2240);
    for (int i = 0; i < 2240; i++) {
        ans[i] = local_ans[i];
    }
}

// Gets child (or parent) pid from child (or parent) index
short State::pid_from_child_index(short child_index) {
    assert(-1 <= child_index);
    assert(child_index <= State::num_children);
    if (child_index == State::num_children) {
        // Is parent
        return State::parent_id;
    }
    return (State::pid * State::branching_factor) + child_index + 1;
}

// Gets child (or parent) index from child (or parent) pid
short State::child_index_from_pid(short child_pid) {
    assert(0 <= child_pid && child_pid <= State::nprocs);
    if (child_pid == State::parent_id) {
        return State::num_children;
    }
    return child_pid - 1 - (State::pid * State::branching_factor);
}

// Prints out the current progress of the solver
void State::print_progress(Cnf &cnf, Deque &task_stack) {
    unsigned int assigned_variables = cnf.num_vars_assigned;
    unsigned int unassigned_variables = cnf.num_variables - assigned_variables;
    unsigned int remaining_clauses = cnf.clauses.num_indexed - cnf.clauses.num_clauses_dropped;
    printf("PID %d: [ depth %d || %d unassigned variables || %d remaining clauses || %d size work stack ]\n", State::pid, cnf.depth, unassigned_variables, remaining_clauses, task_stack.count);
}

// Returns whether there are any other processes requesting our work
bool State::workers_requesting() {
    return State::num_requesting > 0;
}

// Returns whether we should forward an urgent request
bool State::should_forward_urgent_request() {
    if (State::pid == 0) {
        return (State::num_urgent == State::num_children - 1);
    }
    return (State::num_urgent == State::num_children);
}

// Returns whether we should implicitly abort due to urgent requests
bool State::should_implicit_abort() {
    if (State::pid == 0) {
        return (State::num_urgent == State::num_children);
    }
    return (State::num_urgent == State::num_children + 1);
}

// Returns whether the state is able to supply work to requesters
bool State::can_give_work(Deque task_stack, Interconnect interconnect) {
    return ((State::num_non_trivial_tasks + interconnect.num_stashed_work) > 1);
}

// Ensures the task stack is a valid one
bool State::task_stack_invariant(
        Cnf &cnf, 
        Deque &task_stack, 
        int supposed_num_tasks) 
    {
    assert((supposed_num_tasks == 0) || !backtrack_at_top(task_stack));
    if (State::num_non_trivial_tasks > 0) {
        assert(task_stack.count > 0);
    }
    int num_processed = 0;
    int num_to_process = task_stack.count;
    int true_num_tasks = 0;
    while (num_processed < num_to_process) {
        void *current_ptr = task_stack.pop_from_front();
        Task current = *((Task *)current_ptr);
        if (!current.is_backtrack) {
            // This is a non-trivial task, increment
            true_num_tasks++;
        }
        task_stack.add_to_back(current_ptr);
        num_processed++;
    }
    assert(task_stack.count == num_to_process);
    void **task_list = task_stack.as_list();
    void **thief_list = (*State::thieves).as_list();
    int num_thieves = (*State::thieves).count;
    int total_assigned = 0;
    int num_queued = 0;
    int num_stolen = 0;
    int reported_num_local = 0;
    int reported_num_queued = 0;
    int reported_num_remote = 0;
    int reported_num_stolen = 0;
    for (int i = 0; i < task_stack.count; i++) {
        void *task_ptr = task_list[i];
        Task task = *((Task *)task_ptr);
        if (!task.is_backtrack) {
            Assignment task_assignment;
            task_assignment.var_id = task.var_id;
            task_assignment.value = task.assignment;
            char task_status = cnf.get_decision_status(task_assignment);
            assert(task_status == 'q');
            Assignment opposite_assignment;
            opposite_assignment.var_id = task.var_id;
            opposite_assignment.value = !task.assignment;
            char opposite_status = cnf.get_decision_status(opposite_assignment);
            if (task.implier == -1) {
                // Must be a decided (not unit propagated) variable
                if (!((opposite_status == 'q') 
                    || (opposite_status == 'l') 
                    || (opposite_status == 's')
                    || (opposite_status == 'u'))) {
                        cnf.print_task_stack("Failed task stack", task_stack, 1000);
                        printf("Failed, opposite status = %c, id = %d\n", opposite_status, task.var_id);
                    }
                assert((opposite_status == 'q') 
                    || (opposite_status == 'l') 
                    || (opposite_status == 's')
                    || (opposite_status == 'u'));
            } else {
                // Must be a unit propagated variable
                assert(opposite_status == 'u');
            }
            num_queued++;
        }
    }
    for (int var_id = 0; var_id < cnf.num_variables; var_id++) {
        if (cnf.assigned_true[var_id]) {
            if (cnf.assigned_false[var_id]) {
                printf("Var %d both true and false\n", var_id);
            }
            assert(!cnf.assigned_false[var_id]);
            assert((cnf.true_assignment_statuses[var_id] == 'l')
                || (cnf.true_assignment_statuses[var_id] == 'r'));
            assert((cnf.false_assignment_statuses[var_id] == 'u')
                || (cnf.false_assignment_statuses[var_id] == 'q')
                || (cnf.false_assignment_statuses[var_id] == 's'));
            total_assigned++;
        } else if (cnf.assigned_false[var_id]) {
            assert(!cnf.assigned_true[var_id]);
            assert((cnf.false_assignment_statuses[var_id] == 'l')
                || (cnf.false_assignment_statuses[var_id] == 'r'));
            assert((cnf.true_assignment_statuses[var_id] == 'u')
                || (cnf.true_assignment_statuses[var_id] == 'q')
                || (cnf.true_assignment_statuses[var_id] == 's'));
            total_assigned++;
        } else {
            assert((cnf.false_assignment_statuses[var_id] == 'u')
                || (cnf.false_assignment_statuses[var_id] == 'q')
                || (cnf.false_assignment_statuses[var_id] == 's'));
            assert((cnf.true_assignment_statuses[var_id] == 'u')
                || (cnf.true_assignment_statuses[var_id] == 'q')
                || (cnf.true_assignment_statuses[var_id] == 's'));
        }
        Assignment left_assignment;
        left_assignment.var_id = var_id;
        left_assignment.value = true;
        char left_status = cnf.get_decision_status(left_assignment);
        Assignment right_assignment;
        right_assignment.var_id = var_id;
        right_assignment.value = false;
        char right_status = cnf.get_decision_status(right_assignment);
        std::list<char> statuses = {left_status, right_status};
        for (char status : statuses) {
            switch (status) {
                case 'l': {
                    reported_num_local++;
                    break;
                } case 'q': {
                    reported_num_queued++;
                    break;
                } case 'r': {
                    reported_num_remote++;
                    break;
                } case 's': {
                    reported_num_stolen++;
                    break;
                }
            }
        }
    }
    // for (int i = 0; i < num_thieves; i++) {
    //     void *theif_ptr = thief_list[i];
    //     GivenTask stolen_task = *((GivenTask *)theif_ptr);
    //     Assignment task_assignment;
    //     task_assignment.var_id = stolen_task.var_id;
    //     task_assignment.value = stolen_task.assignment;
    //     char task_status = cnf.get_decision_status(task_assignment);
    //     if (task_status != 's') {
    //         printf("%sPID %d: failing thief %d [%d = %d] with task status %c\n", cnf.depth_str.c_str(), State::pid, stolen_task.pid, task_assignment.var_id, (int)task_assignment.value, task_status);
    //     }
    //     assert(task_status == 's');
    //     num_stolen++;
    // }
    assert(num_queued <= reported_num_queued);
    // assert(num_stolen == reported_num_stolen);
    assert(total_assigned == reported_num_local + reported_num_remote);
    short *clause_sizes = (short *)calloc(sizeof(short), 
        cnf.clauses.max_indexable + cnf.clauses.max_conflict_indexable);
    memset(clause_sizes, -1, 
        (cnf.clauses.max_indexable + cnf.clauses.max_conflict_indexable) 
        * sizeof(short));
    if (cnf.eedit_stack.count > 0) {
        Deque local_edits = (*((Deque *)(cnf.eedit_stack.peak_front())));
        void **edit_list = (local_edits).as_list();
        bool found_decided_variable_assignment = false;
        for (int i = 0; i < local_edits.count; i++) {
            void *edit_ptr = edit_list[i];
            FormulaEdit edit = *((FormulaEdit *)edit_ptr);
            int clause_id = edit.edit_id;
            if (edit.edit_type == 'c') { // Clause drop
                assert(0 <= clause_id && clause_id < cnf.clauses.max_indexable + cnf.clauses.max_conflict_indexable);
                // No size changing clauses after dropping them
                assert(clause_sizes[clause_id] == -1);
                clause_sizes[clause_id] = 0;
            } else if (edit.edit_type == 's') { // Size decrease
                assert(0 <= clause_id && clause_id < cnf.clauses.max_indexable + cnf.clauses.max_conflict_indexable);
                // No duplicate size change edits
                // Size should change always
                assert(edit.size_before != -1);
                assert(clause_sizes[clause_id] < edit.size_before);
                clause_sizes[clause_id] = edit.size_before;
            } else {
                if (cnf.variables[edit.edit_id].implying_clause_id == -1) {
                    assert(!found_decided_variable_assignment);
                    found_decided_variable_assignment = true;
                }
            }
        }
        if (local_edits.count > 0) {
            free(edit_list);
        }
    }
    if (task_stack.count > 0) {
        free(task_list);
    }
    if ((*State::thieves).count > 0) {
        free(thief_list);
    }
    return true;
}

// Applies an edit to the given compressed CNF
void State::apply_edit_to_compressed(
        Cnf &cnf,
        unsigned int *compressed, 
        FormulaEdit edit) 
    {
    switch (edit.edit_type) {
        case 'v': {
            // Add variable set to it's int offset
            int variable_id = edit.edit_id;
            VariableLocations location = cnf.variables[variable_id];
            unsigned int mask_to_add = location.variable_addition;
            unsigned int offset;
            if (cnf.assigned_true[variable_id]) {
                offset = location.variable_true_addition_index;
            } else {
                assert(cnf.assigned_false[variable_id]);
                offset = location.variable_false_addition_index;
            }
            compressed[offset] += mask_to_add;
            return;
        } case 'c': {
            // Add clause drop to it's int offset
            int clause_id = edit.edit_id;
            if (clause_id >= cnf.clauses.max_indexable) return; //don't do for conflict clauses
            Clause clause = cnf.clauses.get_clause(clause_id);
            unsigned int mask_to_add = clause.clause_addition;
            unsigned int offset = clause.clause_addition_index;
            compressed[offset] += mask_to_add;
            return;
        } default: {
            return;
        }
    }
}

// Grabs work from the top of the task stack, updates Cnf structures
void *State::grab_work_from_stack(
        Cnf &cnf,
        Deque &task_stack,
        short recipient_pid)
    {
    if (PRINT_LEVEL > 0) printf("PID %d: grabbing work from stack\n", State::pid);
    print_data(cnf, task_stack, "grabbing work from stack");
    assert(State::num_non_trivial_tasks > 1);
    assert(task_stack.count >= State::num_non_trivial_tasks);
    // Get the actual work as a copy
    void *top_task_ptr = task_stack.pop_from_back();
    Task top_task = *((Task *)(top_task_ptr));
    free(top_task_ptr);
    void *work = cnf.convert_to_work_message(cnf.oldest_compressed, top_task);
    if (top_task.assignment) {
        cnf.true_assignment_statuses[top_task.var_id] = 's';
    } else {
        cnf.false_assignment_statuses[top_task.var_id] = 's';
    }
    GivenTask task_to_give;
    task_to_give.var_id = top_task.var_id;
    task_to_give.pid = recipient_pid;
    task_to_give.assignment = top_task.assignment;
    GivenTask *task_to_give_ptr = (GivenTask *)malloc(sizeof(GivenTask));
    *task_to_give_ptr = task_to_give;
    (*State::thieves).add_to_front((void *)task_to_give_ptr);
    // Prune the top of the tree
    while (backtrack_at_top(task_stack)) {
        assert(cnf.eedit_stack.count > 0);
        // Two deques loose their top element, one looses at least one element
        Deque edits_to_apply = *((Deque *)((cnf.eedit_stack).pop_from_back()));
        while (edits_to_apply.count > 0) {
            void *formula_edit_ptr = edits_to_apply.pop_from_back();
            FormulaEdit edit = *((FormulaEdit *)formula_edit_ptr);
            free(formula_edit_ptr);
            apply_edit_to_compressed(cnf, cnf.oldest_compressed, edit);
        }
        edits_to_apply.free_deque();
        // Ditch the backtrack task at the top
        task_stack.pop_from_back();
    }
    State::num_non_trivial_tasks--;
    assert(State::num_non_trivial_tasks >= 1);
    if (PRINT_LEVEL > 1) printf("PID %d: grabbing work from stack done\n", State::pid);
    print_data(cnf, task_stack, "grabbed work from stack");
    if (PRINT_LEVEL > 5) print_compressed(
        cnf.pid, "giving work", cnf.depth_str, (unsigned int *)work, cnf.work_ints);
    return work;
}

// Picks recipient index to give work to
short State::pick_work_recipient() {
    // P0 has it's Parent urgently requesi
    short recipient_index;
    for (short child = 0; child <= State::num_children; child++) {
        if (State::num_urgent > 0) {
            // Urgent requests take precedence
            // Pick an urgently-requesting child (or parent)
            if (State::child_statuses[child] == 'r'
                || State::child_statuses[child] == 'w') {
                // Non-urgent
                continue;
            }
        } else {
            // Pick a requesting child (or parent)
            if (State::child_statuses[child] == 'w') {
                // Not requesting
                continue;
            }
        }
        if (PRINT_LEVEL > 0) printf("PID %d: picked work recipient %d\n", State::pid, pid_from_child_index(child));
        return child;
    }
    assert(false);
    return 0;
}

// Picks recipient index to ask for work
short State::pick_request_recipient() {
    bool make_double_requests = should_forward_urgent_request();
    short recipient_index = -1;
    // Pick recipient we haven't already asked, prefering one who hasn't asked
    // us, with bias for asking our parent (hence reverse direction).
    for (short child = State::num_children; child >= 0; child--) {
        if (make_double_requests) {
            if (State::requests_sent[child] == 'u') {
                // We've urgently-asked them already
                continue;
            }
        } else {
            if (State::requests_sent[child] != 'n') {
                // We've asked them already
                continue;
            }
        }
        if (State::child_statuses[child] == 'u') {
            // Can't ask them, they're out of work entirely
            continue;
        }
        recipient_index = child;
        if (State::child_statuses[child] == 'w') {
            // They're not asking us for work, stop here
            break;
        }
    }
    if (PRINT_LEVEL > 0) printf("PID %d: picked request recipient %d\n", State::pid, pid_from_child_index(recipient_index));
    return recipient_index;
}

// Gives one unit of work to lazy processors
void State::give_work(
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    if (PRINT_LEVEL > 0) printf("PID %d: giving work\n", State::pid);
    short recipient_index = pick_work_recipient();
    assert(State::child_statuses[recipient_index] != 'w');
    short recipient_pid = pid_from_child_index(recipient_index);
    void *work;
    // Prefer to give stashed work
    if (interconnect.have_stashed_work()) {
        if (interconnect.have_stashed_work(recipient_pid)) {
            work = (interconnect.get_stashed_work(recipient_pid)).data;
        } else {
            work = (interconnect.get_stashed_work()).data;
        }
    } else {
        assert(task_stack.count > 0);
        work = grab_work_from_stack(cnf, task_stack, recipient_pid);
    }
    if (State::child_statuses[recipient_index] == 'u') {
        State::num_urgent--;
    }
    State::child_statuses[recipient_index] = 'w';
    State::num_requesting--;
    interconnect.send_work(recipient_pid, work);
}

// Gets stashed work, returns true if any was grabbed
bool State::get_work_from_interconnect_stash(
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    // NICE: Perhaps we get stashed work from a sender who is no longer 
    // asking us for work now (if possible)?
    // Preference for children or parents?
    if (!interconnect.have_stashed_work()) {
        return false;
    }
    if (PRINT_LEVEL > 0) printf("PID %d: getting work from interconnect stash\n", State::pid);
    Message work_message = interconnect.get_stashed_work();
    handle_work_message(
        work_message, cnf, task_stack, interconnect);
    return true;
}

// Returns whether we are out of work to do
bool State::out_of_work() {
    return State::num_non_trivial_tasks == 0;
}

// Asks parent or children for work, called once when we finish our work
void State::ask_for_work(
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    assert(!interconnect.have_stashed_work());
    assert(State::num_urgent <= State::num_children);
    if (should_implicit_abort()) {
        // Self abort
        abort_others(interconnect);
        abort_process(cnf, task_stack, interconnect);
    } else if (should_forward_urgent_request()) {
        if (PRINT_LEVEL > 0) printf("PID %d: urgently asking for work\n", State::pid);
        // Send a single urgent work request
        short dest_index = pick_request_recipient();
        if (dest_index == -1) {
            return;
        }
        short dest_pid = pid_from_child_index(dest_index);
        if (State::requests_sent[dest_index] == 'r') {
            // Send an urgent upgrade
            interconnect.send_work_request(dest_pid, 2);
        } else {
            assert(State::requests_sent[dest_index] == 'n');
            // Send an urgent request
            interconnect.send_work_request(dest_pid, 1);
        }
        State::requests_sent[dest_index] = 'u';
    } else {
        if (PRINT_LEVEL > 0) printf("PID %d: asking for work\n", State::pid);
        // Send a single work request
        short dest_index = pick_request_recipient();
        if (dest_index == -1) {
            return;
        }
        short dest_pid = pid_from_child_index(dest_index);
        assert(State::requests_sent[dest_index] == 'n');
        interconnect.send_work_request(dest_pid, 0);
        State::requests_sent[dest_index] = 'r';
    }
}

// Invalidates (erases) ones work
void State::invalidate_work(Deque &task_stack) {
    assert(false); // unfinished
    // TODO: undo all local edits as well
    State::num_non_trivial_tasks = 0;
    task_stack.free_data();
}

// Empties/frees data structures and immidiately returns
void State::abort_process(
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect,
        bool explicit_abort) 
    {
    if (PRINT_LEVEL > 0) printf("PID %d: aborting process\n", State::pid);
    State::process_finished = true;
    State::was_explicit_abort = explicit_abort;
    if (cnf.is_sudoku) {
        cnf.sudoku_board = cnf.get_sudoku_board();
    }
    cnf.free_cnf();
    interconnect.free_interconnect();
    free(State::child_statuses);
    free(State::requests_sent);
    (*State::thieves).free_deque();
    free(State::thieves);
    task_stack.free_deque();
}

// Sends messages to children to force them to abort
void State::abort_others(Interconnect &interconnect, bool explicit_abort) {
    if (explicit_abort) {
        printf("PID %d: explicitly aborting others\n", State::pid);
        // Success, broadcase explicit abort to every process
        for (short i = 0; i < State::nprocs; i++) {
            if (i == State::pid) {
                continue;
            }
            interconnect.send_abort_message(i);
        }
    } else {
        printf("PID %d: aborting others\n", State::pid);
        // Any children who did not receive an urgent request from us should
        assert(should_implicit_abort());
        for (short child = 0; child <= State::num_children; child++) {
            short recipient = pid_from_child_index(child);
            if (State::requests_sent[child] != 'u') {
                // Child needs ones
                if (State::requests_sent[child] == 'n') {
                    // Normal urgent request
                    printf("PID %d sending urgent req to %d\n", State::pid, (int)recipient);
                    interconnect.send_work_request(recipient, 1);
                } else {
                    // Urgent upgrade
                    printf("PID %d sending urgent upgrade to %d\n", State::pid, (int)recipient);
                    interconnect.send_work_request(recipient, 2);
                }
                State::requests_sent[child] = 'u';
            } else {
                printf("PID %d already sent urgent to %d\n", State::pid, (int)recipient);
            }
        }
    }
}

// Handles work received
void State::handle_work_request(
        short sender_pid,
        short version,
        Cnf &cnf,
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    short child_index = child_index_from_pid(sender_pid);
    if (version == 0) { // Normal request
        if (PRINT_LEVEL > 0) printf("PID %d: handling work request from sender %d (child %d)\n", State::pid, sender_pid, child_index);
        assert(State::child_statuses[child_index] == 'w');
        // Log the request
        State::child_statuses[child_index] = 'r';
        State::num_requesting++;
    } else { // Urgent request
        if (version == 1) { // Urgent request
            if (PRINT_LEVEL > 0) printf("PID %d: handling urgent work request from sender %d (child %d)\n", State::pid, sender_pid, child_index);
            assert(State::child_statuses[child_index] == 'w');
            State::num_requesting++;
        } else { // Urgent upgrade
            if (PRINT_LEVEL > 0) printf("PID %d: handling urgent upgrade from sender %d (child %d)\n", State::pid, sender_pid, child_index);
            assert(State::child_statuses[child_index] != 'u');
            if (State::child_statuses[child_index] == 'w') {
                // Effectively a no-op, we already handled the non-urgent 
                // request that came before
                return;
            }
        }
        // Log the request
        State::child_statuses[child_index] = 'u';
        State::num_urgent++;
        // Check if we need to forward urgent, or abort
        if (out_of_work()) {        
            if (should_implicit_abort()) {
                // Self-abort
                printf("implicit abort from pid %d (in handle_work_request)\n", pid);
                abort_others(interconnect);
                abort_process(cnf, task_stack, interconnect);
            } else if (should_forward_urgent_request()) {
                // Send a single urgent work request
                short dest_index = pick_request_recipient();
                short dest_pid = pid_from_child_index(dest_index);
                if (State::requests_sent[dest_index] == 'r') {
                    // Send an urgent upgrade
                    interconnect.send_work_request(dest_pid, 2);
                } else {
                    assert(State::requests_sent[dest_index] == 'n');
                    // Send an urgent request
                    interconnect.send_work_request(dest_pid, 1);
                }
                State::requests_sent[dest_index] = 'u';
            }
        }
    }
}

// Handles work received
void State::handle_work_message(
        Message message,
        Cnf &cnf,
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    if (PRINT_LEVEL > 0) printf("PID %d: handling work message\n", State::pid);
    short sender_pid = message.sender;
    short child_index = child_index_from_pid(sender_pid);
    void *work = message.data;
    assert(State::requests_sent[child_index] != 'n');
    assert(child_statuses[child_index] != 'u');
    assert(!interconnect.have_stashed_work(sender_pid));
    if (out_of_work()) {
        // Reconstruct state from work
        Task task = cnf.extract_task_from_work(work);
        State::current_task.assignment = task.assignment;
        State::current_task.var_id = task.var_id;
        // NICE: implement forwarding
        State::current_task.pid = sender_pid;
        cnf.reconstruct_state(work, task_stack);
        (*State::thieves).free_data();
        State::num_non_trivial_tasks = 1;
        print_data(cnf, task_stack, "Post reconstruct");

        // DEBUG
        int ct = 0;
        bool *true_assn = (bool*)calloc(sizeof(bool), cnf.num_variables);
        for(int i = 0; i < 2240; i++) {
            true_assn[State::ans[i]] = true;
        }
        for (int i = 0; i < cnf.num_variables; i++) {
            if (i == task.var_id) {
                if (task.assignment == true_assn[i]) {
                    ct++;
                    continue;
                } else {
                    break;
                }
            }
            if (!((cnf.assigned_false[i] && true_assn[i]) && (cnf.assigned_true[i] && !true_assn[i]))) {
                ct++;
            } else {
                break;
            }
        }
        free(true_assn);
        printf("pid %d got work now\n", pid);
        if (ct == cnf.num_variables) printf("SHOULD BE CORRECT AT PID %d\n", pid);

    } else {
        // Add to interconnect work stash
        interconnect.stash_work(message);
    }
    State::requests_sent[child_index] = 'n';
}

// Handles an abort message, possibly forwarding it
void State::handle_message(
        Message message, 
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    if (PRINT_LEVEL > 0) printf("PID %d: handling message\n", State::pid);
    assert(0 <= State::num_urgent && State::num_urgent <= State::num_children);
    assert(State::num_urgent <= State::num_requesting);
    assert(State::num_requesting <= State::num_children + 1);
    assert(0 <= message.type <= 4);
    switch (message.type) {
        case 3: {
            handle_work_message(
                message, cnf, task_stack, interconnect);
            return;
        } case 4: {
            abort_process(cnf, task_stack, interconnect, true);
            return;
        } case 5: {
            invalidate_work(task_stack);
            return;
        } case 6: {
            assert(SEND_CONFLICT_CLAUSES);
            Clause conflict_clause = message_to_clause(message);
            handle_remote_conflict_clause(
                cnf, 
                task_stack, 
                conflict_clause, 
                interconnect);
            return;
        } default: {
            // 0, 1, or 2
            handle_work_request(
                message.sender, message.type, cnf, task_stack, interconnect);
            return;
        }
    }
    assert(0 <= State::num_urgent 
        && State::num_urgent <= State::num_children + 1);
    assert(State::num_urgent <= State::num_requesting);
    assert(State::num_requesting <= State::num_children + 1);
}

// Edits history to make it appear as though the conflict clause is
// normal.
void State::insert_conflict_clause_history(Cnf &cnf, Clause conflict_clause) {
    if (PRINT_LEVEL > 0) printf("%s\tPID %d: inserting conflict clause into history\n", cnf.depth_str.c_str(), State::pid);
    bool look_for_drop = false;
    cnf.clause_satisfied(conflict_clause, &look_for_drop);
    int drop_var_id = -1;
    IntDeque assigned_literals;
    for (int i = 0; i < conflict_clause.num_literals; i++) {
        int var_id = conflict_clause.literal_variable_ids[i];
        bool literal_sign = conflict_clause.literal_signs[i];
        if (cnf.assigned_true[var_id] 
            && (cnf.true_assignment_statuses[var_id] == 'l')) {
            if (literal_sign) {
                drop_var_id = var_id;
            } else {
                assigned_literals.add_to_front(var_id);
            }
        } else if (cnf.assigned_false[var_id]
            && (cnf.false_assignment_statuses[var_id] == 'l')) {
            if (!literal_sign) {
                drop_var_id = var_id;
            } else {
                assigned_literals.add_to_front(var_id);
            }
        }
    }
    int insertions_made = 0;
    int iterations_checked = 0;
    if (PRINT_LEVEL > 2) printf("%s\tPID %d: need %d insertions for decrement of clause size\n", cnf.depth_str.c_str(), State::pid, assigned_literals.count);
    if (look_for_drop) {
        printf("%s\tPID %d: need drop edit for clause\n", cnf.depth_str.c_str(), State::pid);
    }
    int iterations_to_check = cnf.eedit_stack.count;
    int num_unsat = cnf.get_num_unsat(conflict_clause);
    DoublyLinkedList *iteration_ptr = (*(cnf.eedit_stack.head)).next;
    while ((iterations_checked < iterations_to_check) 
        && (assigned_literals.count > 0)) {
        if (PRINT_LEVEL > 3) printf("%s\t\tPID %d: checking iteration (%d/%d), %d edits left\n", cnf.depth_str.c_str(), State::pid, iterations_checked, iterations_to_check, assigned_literals.count);
        void *iteration_edits_ptr = (*iteration_ptr).value;
        Deque iteration_edits = (*((Deque *)iteration_edits_ptr));
        int current_count = iteration_edits.count;
        int edits_seen = 0;
        DoublyLinkedList *edit_element_ptr = (*iteration_edits.head).next;
        while ((edits_seen < current_count) && (assigned_literals.count > 0)) {
            if (PRINT_LEVEL > 4) printf("%s\t\t\tPID %d: inspecting edit (%d/%d), %d edits left\n", cnf.depth_str.c_str(), State::pid, edits_seen, current_count, assigned_literals.count);
            void *edit_ptr = (*edit_element_ptr).value;
            FormulaEdit current_edit = *((FormulaEdit *)edit_ptr);
            if (current_edit.edit_type == 'v') {
                int var_id = current_edit.edit_id;
                if (look_for_drop && var_id == drop_var_id) {
                    // Add the drop edit
                    void *drop_edit = clause_edit(conflict_clause.id);
                    DoublyLinkedList *prev = (*edit_element_ptr).prev;
                    DoublyLinkedList drop_element;
                    drop_element.value = drop_edit;
                    drop_element.prev = prev;
                    drop_element.next = edit_element_ptr;
                    DoublyLinkedList *drop_element_ptr = (
                        DoublyLinkedList *)malloc(sizeof(DoublyLinkedList));
                    *drop_element_ptr = drop_element;
                    (*prev).next = drop_element_ptr;
                    (*edit_element_ptr).prev = drop_element_ptr;
                    look_for_drop = false;
                    insertions_made++;
                    iteration_edits.count++;
                    (*((Deque *)iteration_edits_ptr)).count++;
                    if (PRINT_LEVEL > 5) printf("%s\t\t\tPID %d: inserted drop edit\n", cnf.depth_str.c_str(), State::pid);
                } else {
                    int num_to_check = assigned_literals.count;
                    while (num_to_check > 0) {
                        int literal_var_id = assigned_literals.pop_from_front();
                        if (literal_var_id == var_id) {
                            // Drecrease clause size edit
                            void *size_change = size_change_edit(
                                conflict_clause.id, num_unsat + 1, num_unsat);
                            DoublyLinkedList *prev = (*edit_element_ptr).prev;
                            DoublyLinkedList size_change_element;
                            size_change_element.value = size_change;
                            size_change_element.prev = prev;
                            size_change_element.next = edit_element_ptr;
                            DoublyLinkedList *size_change_element_ptr = (
                                DoublyLinkedList *)malloc(
                                    sizeof(DoublyLinkedList));
                            *size_change_element_ptr = size_change_element;
                            (*prev).next = size_change_element_ptr;
                            (*edit_element_ptr).prev = size_change_element_ptr;
                            num_unsat++;
                            insertions_made++;
                            iteration_edits.count++;
                            (*((Deque *)iteration_edits_ptr)).count++;
                            if (PRINT_LEVEL > 5) printf("%s\t\t\tPID %d: inserted decrement edit\n", cnf.depth_str.c_str(), State::pid);
                            break;
                        } else {
                            assigned_literals.add_to_back(literal_var_id);
                            num_to_check--;
                        }
                    }
                }
            } else {
                if (PRINT_LEVEL > 5) printf("%s\t\t\tPID %d: non-variable edit ignored\n", cnf.depth_str.c_str(), State::pid);
                assert(current_edit.edit_id != conflict_clause.id);
            }
            edit_element_ptr = (*edit_element_ptr).next;
            edits_seen++;
        }
        iterations_checked++;
        iteration_ptr = (*iteration_ptr).next;
    }
    // assert(assigned_literals.count == 0); // should be the number of remotely assigned vars?

    // Restore the change we made
    if (PRINT_LEVEL > 1) printf("%s\tPID %d: made %d insertions into conflict clause history\n", cnf.depth_str.c_str(), State::pid, insertions_made);
    if (PRINT_LEVEL > 2) cnf.print_edit_stack("new edit stack", (cnf.eedit_stack));
    assigned_literals.free_deque();
}

// Sends messages to specified theives in light of conflict
void State::inform_thieves_of_conflict(
        Deque selected_thieves,
        Clause conflict_clause,
        Interconnect &interconnect,
        bool invalidate)
    {
    for (int i = 0; i < selected_thieves.count; i++) {
        void *thief_ptr = selected_thieves.pop_from_front();
        GivenTask thief_task = *((GivenTask *)thief_ptr);
        selected_thieves.add_to_back(thief_ptr);
        short recipient_pid = thief_task.pid;
        if (invalidate) {
            interconnect.send_invalidation(recipient_pid);
        } else {
            interconnect.send_conflict_clause(recipient_pid, conflict_clause);
        }
    }
}

// Slits thieves based on midpoint (not included), populates results
void State::split_thieves(
        Task midpoint, 
        Deque &thieves_before, 
        Deque &thieves_after) 
    {
    bool seen_midpoint = false;
    short num_thieves = (*State::thieves).count;
    for (int i = 0; i < num_thieves; i++) {
        void *thief_ptr = (*State::thieves).pop_from_front();
        GivenTask thief_task = *((GivenTask *)thief_ptr);
        (*State::thieves).add_to_back(thief_ptr);
        if ((thief_task.var_id == midpoint.var_id) 
            && (thief_task.assignment == midpoint.assignment)) {
            seen_midpoint = true;
            // Don't include midpoint
            continue;
        }
        if (seen_midpoint) {
            thieves_after.add_to_back(thief_ptr);
        } else {
            thieves_before.add_to_back(thief_ptr);
        }
    }
}

// Moves to lowest point in call stack when conflict clause is useful
void State::backtrack_to_conflict_head(
        Cnf &cnf, 
        Deque &task_stack,
        Clause conflict_clause,
        Task lowest_bad_decision) 
    {
    if (PRINT_LEVEL > 0) printf("%sPID %d: backtrack to conflict head with lowest decision at var %d\n", cnf.depth_str.c_str(), State::pid, lowest_bad_decision.var_id);
    if (PRINT_LEVEL > 1) cnf.print_task_stack("Pre conflict backtrack", task_stack);
    if (PRINT_LEVEL > 2) cnf.print_edit_stack("Pre conflict backtrack", (cnf.eedit_stack));
    assert(cnf.get_num_unsat(conflict_clause) == 0);
    int num_backtracks = 0;
    assert(task_stack.count > 0);
    Task current_task;
    bool make_initial_backtrack = false;
    if (task_stack.count >= 2) {
        Task first_task = peak_task(task_stack);
        Task second_task = peak_task(task_stack, 1);
        Task opposite_first = opposite_task(first_task);
        if (cnf.get_decision_status(opposite_first) == 'l') {
            make_initial_backtrack = true;
        }
    } else {
        make_initial_backtrack = true;
    }
    if (make_initial_backtrack) {
        printf("Backtrack initial\n");
        cnf.backtrack();
        num_backtracks++;
    }
    bool tasks_equal = false;
    while ((!tasks_equal) && (task_stack.count > 0)) {
        // Clause is not useful to us, must backtrack more
        current_task = get_task(task_stack);
        tasks_equal = (lowest_bad_decision.var_id == current_task.var_id)
            && (lowest_bad_decision.is_backtrack == current_task.is_backtrack);
        if (current_task.is_backtrack) {
            if (!tasks_equal) {
                cnf.backtrack();
                num_backtracks++;
            }
        } else {
            if (current_task.assignment) {
                cnf.true_assignment_statuses[current_task.var_id] = 'u';
                if (PRINT_LEVEL >= 3) printf("%sPID %d: removed pre-conflict task {%d = T}\n", cnf.depth_str.c_str(), State::pid, current_task.var_id);
            } else {
                cnf.false_assignment_statuses[current_task.var_id] = 'u';
                if (PRINT_LEVEL >= 3) printf("%sPID %d: removed pre-conflict task {%d = F}\n", cnf.depth_str.c_str(), State::pid, current_task.var_id);
            }
            State::num_non_trivial_tasks--;   
        } 
        if (PRINT_LEVEL > 5) cnf.print_task_stack("During backracking", task_stack);
        if (PRINT_LEVEL > 5) cnf.print_edit_stack("During backracking", (cnf.eedit_stack));
    }
    if (PRINT_LEVEL > 1) printf("%sPID %d: backtracked %d times\n", cnf.depth_str.c_str(), State::pid, num_backtracks);
    cnf.print_task_stack("Post conflict backtrack", task_stack);
    if (PRINT_LEVEL > 3) cnf.print_edit_stack("Post conflict backtrack", (cnf.eedit_stack));
    assert(cnf.get_num_unsat(conflict_clause) >= 1);
}

// Adds tasks based on what a conflict clause says (always greedy)
int State::add_tasks_from_conflict_clause(
        Cnf &cnf, 
        Deque &task_stack, 
        Clause conflict_clause) 
    {
    if (PRINT_LEVEL > 3) printf("%sPID %d: adding tasks from conflict clause\n", cnf.depth_str.c_str(), State::pid);
    int new_var_id;
    bool new_var_sign;
    int num_unsat = cnf.pick_from_clause(
        conflict_clause, &new_var_id, &new_var_sign);
    assert(0 < num_unsat);
    if (PRINT_LEVEL > 1) printf("%sPID %d: picked new var %d from clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_var_id, conflict_clause.id, cnf.clause_to_string_current(conflict_clause, false).c_str());
    if (num_unsat == 1) {
        void *only_task = make_task(new_var_id, conflict_clause.id, new_var_sign);
        task_stack.add_to_front(only_task);
        State::num_non_trivial_tasks++;
        if (new_var_sign) {
            cnf.true_assignment_statuses[new_var_id] = 'q';
        } else {
            cnf.false_assignment_statuses[new_var_id] = 'q';
        }
        if (PRINT_LEVEL >= 3) printf("%sPID %d: variable %d unit propped from conflict clause %d\n", cnf.depth_str.c_str(), State::pid, new_var_id, conflict_clause.id);
        return 1;
    } else {
        // Add a backtrack task to do last
        if (task_stack.count > 0) {
            void *backtrack_task = make_task(new_var_id, -1, true, true);
            task_stack.add_to_front(backtrack_task);
        }
        bool first_choice = new_var_sign;
        void *important_task = make_task(new_var_id, -1, first_choice);
        task_stack.add_to_front(important_task);
        State::num_non_trivial_tasks += 2;
        if (first_choice) {
            cnf.true_assignment_statuses[new_var_id] = 'q';
            cnf.false_assignment_statuses[new_var_id] = 'u';
        } else {
            cnf.true_assignment_statuses[new_var_id] = 'u';
            cnf.false_assignment_statuses[new_var_id] = 'q';
        }
        if (PRINT_LEVEL >= 3) printf("%sPID %d: variable %d explicitly decided from conflict clause %d\n", cnf.depth_str.c_str(), State::pid, new_var_id, conflict_clause.id);
        return 2;
    }
}

// Adds a conflict clause to the clause list
// EDITED: NO LONGER PICKS FROM CLAUSE
void State::add_conflict_clause(
        Cnf &cnf, 
        Clause conflict_clause,
        Deque &task_stack,
        bool pick_from_clause) 
    {
    if (PRINT_LEVEL > 0) printf("%sPID %d: adding conflict clause\n", cnf.depth_str.c_str(), State::pid);
    if (PRINT_LEVEL > 2) cnf.print_cnf("Before conflict clause", cnf.depth_str, (PRINT_LEVEL <= 3));
    if (PRINT_LEVEL > 2) cnf.print_task_stack("Before conflict clause", task_stack);
    if (PRINT_LEVEL > 2) printf("Conflict clause num unsat = %d\n", cnf.get_num_unsat(conflict_clause));
    assert(clause_is_sorted(conflict_clause));
    int new_clause_id = cnf.clauses.max_indexable + cnf.clauses.num_conflict_indexed;
    conflict_clause.id = new_clause_id;
    insert_conflict_clause_history(cnf, conflict_clause);
    for (int lit = 0; lit < conflict_clause.num_literals; lit++) {
        int var_id = conflict_clause.literal_variable_ids[lit];
        (cnf.variables[var_id]).clauses_containing.add_to_back(
            new_clause_id);
    }
    cnf.clauses.add_conflict_clause(conflict_clause);
    int actual_size = cnf.get_num_unsat(conflict_clause);
    if (actual_size == 0) {
        cnf.clauses.drop_clause(new_clause_id);
    } else if (actual_size != conflict_clause.num_literals) {
        if (PRINT_LEVEL > 2) printf("Changing conflict clause size to %d\n", actual_size);
        cnf.clauses.change_clause_size(conflict_clause.id, actual_size);
    }
    if (PRINT_LEVEL > 2) cnf.print_cnf("With conflict clause", cnf.depth_str, true);
    if (PRINT_LEVEL > 2) cnf.print_task_stack("With conflict clause", task_stack);
}

// Returns who is responsible for making the decision that runs contrary
// to decided_conflict_literals, one of
// 'l' (in local), 'r' (in remote), or 's' (in stealers).
// Also populates the lowest (most-recent) bad decision made.
char State::blame_decision(
        Cnf &cnf,
        Deque &task_stack,
        Deque decided_conflict_literals, 
        Task *lowest_bad_decision) 
    {
    if (PRINT_LEVEL > 1) printf("%sPID %d: blaming decision\n", cnf.depth_str.c_str(), State::pid);
    char result = 'r';
    if (decided_conflict_literals.count == 0) {
        if (task_stack.count == 0) {
            return result;
        } else if (task_stack.count == 1) {
            Task top_task = peak_task(task_stack);
            top_task.assignment = !top_task.assignment;
            *lowest_bad_decision = top_task;
            return 'l';
        }
        Task top_task = peak_task(task_stack);
        if (!top_task.is_backtrack) {
            top_task = peak_task(task_stack, 1);
        }
        top_task.assignment = !top_task.assignment;
        *lowest_bad_decision = top_task;
        return 'l';
    }
    void **decided_literals_list = decided_conflict_literals.as_list();
    for (int i = 0; i < decided_conflict_literals.count; i++) {
        void *decision_ptr = decided_literals_list[i];
        Assignment good_decision = *((Assignment *)decision_ptr);
        Assignment bad_decision;
        bad_decision.var_id = good_decision.var_id;
        bad_decision.value = !good_decision.value;
        char good_status = cnf.get_decision_status(good_decision);
        char bad_status = cnf.get_decision_status(bad_decision);
        // Then conflict clause is satisfied
        assert(good_status != 'l');
        // Ensure the conflict clause is unsatisfiable
        assert((bad_status == 'l') || (bad_status == 'r'));
        if (bad_status == 'l') {
            if (good_status == 'u') {
                // Should be queued or stolen
                assert(false);
            } else if (good_status == 'q') {
                result = 'l';
                break;
            } else if (good_status == 'r') {
                // Can't locally assign something contrary to remote assignment
                assert(false);
            } else { // 's'
                if (result == 'r') {
                    result = 's';
                }
            }
        } else { // 'r'
            assert(good_status == 'u');
        }
    }
    if (result == 'l') {
        if (PRINT_LEVEL > 2) printf("%sPID %d: searching for local culprit\n", cnf.depth_str.c_str(), State::pid);
        cnf.print_task_stack("should be in", task_stack);
        // Find lowest bad decision locally (good one is queued up)
        void **tasks = task_stack.as_list();
        int num_tasks = task_stack.count;
        // Check each task in order
        for (int task_num = 0; task_num < num_tasks; task_num++) {
            void *task_ptr = tasks[task_num];
            Task task = *((Task *)task_ptr);
            if (task_num == num_tasks - 1) {
                // Don't expect it to be a backtrack task
                if (task.is_backtrack) {
                    continue;
                }
            } else {
                // Should be a backtrack task
                if (!task.is_backtrack) {
                    continue;
                }
            }
            // Check to see if the task is a bad decision
            for (int i = 0; i < decided_conflict_literals.count; i++) {
                void *decision_ptr = decided_literals_list[i];
                Assignment good_decision = *((Assignment *)decision_ptr);
                if (good_decision.var_id == task.var_id) {
                    // Found!
                    Task bad_decision;
                    bad_decision.var_id = task.var_id;
                    bad_decision.assignment = !task.assignment;
                    bad_decision.implier = task.implier;
                    bad_decision.is_backtrack = task.is_backtrack;
                    *lowest_bad_decision = bad_decision;
                    free(decided_literals_list);
                    free(tasks);
                    return result;
                }
            }
        }
        free(tasks);
        assert(false);
    } else if (result == 's') {
        if (PRINT_LEVEL > 2) printf("%sPID %d: searching for stolen culprit\n", cnf.depth_str.c_str(), State::pid);
        // Find lowest bad decision as opposite of lowest stolen good decision 
        void **stolen_tasks = (*State::thieves).as_list();
        int num_tasks = (*State::thieves).count;
        // Check each task in order
        for (int task_num = 0; task_num < num_tasks; task_num++) {
            void *task_ptr = stolen_tasks[task_num];
            GivenTask task = *((GivenTask *)task_ptr);
            // Check to see if the task is a bad decision
            for (int i = 0; i < decided_conflict_literals.count; i++) {
                void *decision_ptr = decided_literals_list[i];
                Assignment good_decision = *((Assignment *)decision_ptr);
                if (good_decision.var_id == task.var_id) {
                    // Found!
                    Task bad_decision;
                    bad_decision.var_id = good_decision.var_id;
                    bad_decision.assignment = !good_decision.value;
                    bad_decision.is_backtrack = false;
                    *lowest_bad_decision = bad_decision;
                    free(decided_literals_list);
                    return result;
                }
            }
        }
    }
    free(decided_literals_list);
    return result;
}

// Handles the current REMOTE conflict clause
void State::handle_remote_conflict_clause(
        Cnf &cnf, 
        Deque &task_stack, 
        Clause conflict_clause,
        Interconnect &interconnect) 
    {
    add_conflict_clause(cnf, conflict_clause, task_stack);

    // test using original valuation
    bool false_so_far = true;
    bool preassigned_false_so_far = true;
    for (int i = 0; i < conflict_clause.num_literals; i++) {
        int lit = conflict_clause.literal_variable_ids[i];
        if (cnf.assigned_true[lit] && conflict_clause.literal_signs[lit]
                  || cnf.assigned_false[lit] && !conflict_clause.literal_signs[lit]) {
            // is true, can add (or ignore?)
            // no need to backtrack
            return;
        } else if (!cnf.assigned_true[lit] && !cnf.assigned_false[lit]) {
            false_so_far = false;
            preassigned_false_so_far = false;
        } else {
            if (cnf.assignment_times[lit] != -1) {
                preassigned_false_so_far = false;
            }
        }
    }

    if (preassigned_false_so_far) {
        invalidate_work(task_stack);
        return;
    }

    if (false_so_far) {
        // unsat -> backtrack
        // what is second-last variable
        std::map<int, int> lit_to_depth;
        for (int i = 0; i < conflict_clause.num_literals; i++) {
            int lit = conflict_clause.literal_variable_ids[i];
            int depth = cnf.assignment_depths[lit];
            lit_to_depth.insert({depth, lit});
        }
        auto iter = lit_to_depth.rbegin();
        int last_d = iter->first;

        // backtrack until I see the first depth
        // TODO: perhaps resolve until it would be unit?
        while (true) {
            Task current_task = get_task(task_stack);

            if (current_task.is_backtrack) {
                cnf.backtrack();
                if (cnf.depth == last_d) {
                    // need to backtrack once more
                    cnf.backtrack(); // could be bug??
                    break;
                }
            } else {
                if (current_task.assignment) {
                    cnf.true_assignment_statuses[current_task.var_id] = 'u';
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: removed pre-conflict task {%d = T}\n", cnf.depth_str.c_str(), State::pid, current_task.var_id);
                } else {
                    cnf.false_assignment_statuses[current_task.var_id] = 'u';
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: removed pre-conflict task {%d = F}\n", cnf.depth_str.c_str(), State::pid, current_task.var_id);
                }
                State::num_non_trivial_tasks--;   
            }
        }
    } else {
        // unassigned, do nothing
    }
}

// Handles a recieved conflict clause
void State::handle_conflict_clause_old(
        Cnf &cnf, 
        Deque &task_stack, 
        Clause conflict_clause,
        Interconnect &interconnect,
        bool blamed_for_error)
    {
    int new_clause_id = cnf.clauses.max_indexable + cnf.clauses.num_conflict_indexed;
    printf("%sPID %d: handle conflict clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_clause_id, cnf.clause_to_string_current(conflict_clause, false).c_str());
    printf("%sPID %d: handle conflict clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_clause_id, cnf.clause_to_string_current(conflict_clause, true).c_str());
    bool clause_value;
    bool clause_satisfied = cnf.clause_satisfied(
        conflict_clause, &clause_value);
    // TODO: handle conflict clause reconstruction & message passing
    if (cnf.clause_exists_already(conflict_clause)) {
        if (PRINT_LEVEL > 1) printf("%s\tPID %d: conflict clause already exists\n", cnf.depth_str.c_str(), State::pid);
        assert(State::nprocs > 1);
        if (blamed_for_error || (clause_satisfied && !clause_value)) {
            // TODO: swap the clause forward
        }
        return;
    } 
    if (!clause_satisfied) {
        if (PRINT_LEVEL > 1) printf("%s\tPID %d: conflict clause unsatisfied\n", cnf.depth_str.c_str(), State::pid);
        add_conflict_clause(cnf, conflict_clause, task_stack);
        return;
    } else if (clause_value) {
        if (PRINT_LEVEL > 1) printf("%s\tPID %d: conflict clause true already\n", cnf.depth_str.c_str(), State::pid);
        print_assignment(State::pid, "current assignment: ", cnf.depth_str.c_str(), cnf.get_assignment(), cnf.num_variables, false, false);
        // TODO: add hist edit here
        add_conflict_clause(cnf, conflict_clause, task_stack);
        return;
    } else {
        if (PRINT_LEVEL > 1) printf("%s\tPID %d: conflict clause must be dealt with\n", cnf.depth_str.c_str(), State::pid);
    }
    // The places to backtrack to are those decided, not unit propagated.
    // We save these beforehand, as backtracking will change whether
    // we believe they are decided or unit propagated.
    Deque decided_conflict_literals = cnf.get_decided_conflict_literals(
        conflict_clause);
    Task lowest_bad_decision;
    char culprit = blame_decision(
        cnf, task_stack, decided_conflict_literals, &lowest_bad_decision);
    if (lowest_bad_decision.is_backtrack) {
        printf("%sPID %d: Lowest bad decision B%d\n", cnf.depth_str.c_str(), State::pid, lowest_bad_decision.var_id);
    } else {
        printf("%sPID %d: Lowest bad decision %d = %d\n", cnf.depth_str.c_str(), State::pid, lowest_bad_decision.var_id, lowest_bad_decision.assignment);
    }
    assert(cnf.valid_lbd(decided_conflict_literals, lowest_bad_decision));
    bool blame_remote = false;
    switch (culprit) {
        case 'l': {
            // local is responsible
            if (PRINT_LEVEL > 1) printf("%s\tPID %d: local is responsible for conflict\n", cnf.depth_str.c_str(), State::pid);
            backtrack_to_conflict_head(
                cnf, task_stack, conflict_clause, lowest_bad_decision);
            add_conflict_clause(cnf, conflict_clause, task_stack, true);
            inform_thieves_of_conflict(
                (*State::thieves), conflict_clause, interconnect);
            break;
        } case 'r': {
            assert(State::nprocs > 1);
            // remote is responsible
            if (PRINT_LEVEL > 1) printf("%s\tPID %d: remote is responsible for conflict\n", cnf.depth_str.c_str(), State::pid);
            invalidate_work(task_stack);
            inform_thieves_of_conflict(
                (*State::thieves), conflict_clause, interconnect, true);
            blame_remote = true;
            break;
        } case 's': {
            assert(State::nprocs > 1);
            // thieves are responsible
            if (PRINT_LEVEL > 1) printf("%s\tPID %d: thieves are responsible for conflict\n", cnf.depth_str.c_str(), State::pid);
            invalidate_work(task_stack);
            Deque thieves_to_invalidate;
            Deque thieves_to_give;
            split_thieves(
                lowest_bad_decision, thieves_to_invalidate, thieves_to_give);
            inform_thieves_of_conflict(
                thieves_to_invalidate, conflict_clause, interconnect, true);
            inform_thieves_of_conflict(
                thieves_to_give, conflict_clause, interconnect);
            break;
        }
    }
    if (State::current_task.pid != -1) {
        // Send conflict clause to remote
        interconnect.send_conflict_clause(
            State::current_task.pid, conflict_clause);
    }
    decided_conflict_literals.free_data();
    if (PRINT_LEVEL > 1) printf("%sPID %d: finished handling conflict clause\n", cnf.depth_str.c_str(), State::pid);
    if (PRINT_LEVEL > 2) cnf.print_task_stack("Updated", task_stack);
}


// Handles the current LOCAL conflict clause
void State::handle_local_conflict_clause(
        Cnf &cnf, 
        Deque &task_stack, 
        Clause conflict_clause,
        Interconnect &interconnect)
    {
    int new_clause_id = cnf.clauses.max_indexable + cnf.clauses.num_conflict_indexed;
    if (PRINT_LEVEL > 2) printf("%sPID %d: handle conflict clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_clause_id, cnf.clause_to_string_current(conflict_clause, false).c_str());
    if (PRINT_LEVEL > 2) printf("%sPID %d: handle conflict clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_clause_id, cnf.clause_to_string_current(conflict_clause, true).c_str());

    // TODO: does this ever happen locally?
    assert(!cnf.clause_exists_already(conflict_clause));
    
    // Backtrack to just when the clause would've become unit [or restart if conflict clause is unit]
    if (conflict_clause.num_literals == 1) {
        // backtrack all the way back
        while (task_stack.count > 0) {
            Task current_task = get_task(task_stack);
            if (current_task.is_backtrack) {
                printf("c\n");
                cnf.backtrack();
            } else {
                if (current_task.assignment) {
                    cnf.true_assignment_statuses[current_task.var_id] = 'u';
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: removed pre-conflict task {%d = T}\n", cnf.depth_str.c_str(), State::pid, current_task.var_id);
                } else {
                    cnf.false_assignment_statuses[current_task.var_id] = 'u';
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: removed pre-conflict task {%d = F}\n", cnf.depth_str.c_str(), State::pid, current_task.var_id);
                }
                State::num_non_trivial_tasks--;   
            }
        }
        cnf.backtrack(); //one more since original task has no backtrack task
    } else {
        // what is second-last variable
        std::map<int, int> lit_to_depth;
        for (int i = 0; i < conflict_clause.num_literals; i++) {
            int lit = conflict_clause.literal_variable_ids[i];
            int depth = cnf.assignment_depths[lit];
            lit_to_depth.insert({depth, lit});
        }
        auto iter = lit_to_depth.rbegin();
        int last_d = iter->first;
        ++iter;
        int second_last_d = iter->first;
        assert(last_d > second_last_d);

        // backtrack until I see the second-latest variable
        while (true) {
            if (task_stack.count == 0) {
                // still need to backtrack, but on "bad" choice of first variable
                cnf.backtrack();
                break;
            }

            Task current_task = get_task(task_stack);

            if (current_task.is_backtrack) {
                cnf.backtrack();
                if (cnf.depth == second_last_d) {
                    break;
                }
            } else {
                if (current_task.assignment) {
                    cnf.true_assignment_statuses[current_task.var_id] = 'u';
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: removed pre-conflict task {%d = T}\n", cnf.depth_str.c_str(), State::pid, current_task.var_id);
                } else {
                    cnf.false_assignment_statuses[current_task.var_id] = 'u';
                    if (PRINT_LEVEL >= 3) printf("%sPID %d: removed pre-conflict task {%d = F}\n", cnf.depth_str.c_str(), State::pid, current_task.var_id);
                }
                State::num_non_trivial_tasks--;   
            }
        }
    }
    if (PRINT_LEVEL > 1) printf("%s\tPID %d: done backJUMPING\n", cnf.depth_str.c_str(), State::pid);
    
    // local is responsible
    if (PRINT_LEVEL > 1) printf("%s\tPID %d: local is responsible for conflict\n", cnf.depth_str.c_str(), State::pid);

    // TODO: where to add conflict clause?
    add_conflict_clause(cnf, conflict_clause, task_stack, false);
    
    if (PRINT_LEVEL > 1) printf("%sPID %d: finished handling conflict clause\n", cnf.depth_str.c_str(), State::pid);
    if (PRINT_LEVEL > 2) cnf.print_task_stack("Updated", task_stack);
    if (SEND_CONFLICT_CLAUSES) {
        interconnect.send_conflict_clause(-1, conflict_clause, true);
    }
}

// Adds one or two variable assignment tasks to task stack
int State::add_tasks_from_formula(Cnf &cnf, Deque &task_stack) {
    if (PRINT_LEVEL > 3) printf("%sPID %d: adding tasks from formula\n", cnf.depth_str.c_str(), State::pid);
    cnf.clauses.reset_iterator();
    Clause current_clause;
    int current_clause_id;
    int new_var_id;
    bool new_var_sign;
    int num_unsat = 2;

    while (true) {
        current_clause = cnf.clauses.get_current_clause();
        current_clause_id = current_clause.id;
        num_unsat = cnf.pick_from_clause(
            current_clause, &new_var_id, &new_var_sign);
        assert(0 < num_unsat);
        if (new_var_id < cnf.n*cnf.n*cnf.n || num_unsat == 1 || !ALWAYS_PREFER_NORMAL_VARS) break;
        cnf.clauses.advance_iterator();
    }

    if (PRINT_LEVEL > 1) printf("%sPID %d: picked new var %d from clause %d %s\n", cnf.depth_str.c_str(), State::pid, new_var_id, current_clause_id, cnf.clause_to_string_current(current_clause, true).c_str());
    if (num_unsat == 1) {
        void *only_task = make_task(new_var_id, current_clause_id, new_var_sign);
        task_stack.add_to_front(only_task);
        State::num_non_trivial_tasks++;
        if (new_var_sign) {
            cnf.true_assignment_statuses[new_var_id] = 'q';
        } else {
            cnf.false_assignment_statuses[new_var_id] = 'q';
        }
        if (PRINT_LEVEL >= 3) printf("%sPID %d: variable %d unit propped\n", cnf.depth_str.c_str(), State::pid, new_var_id);
        return 1;
    } else {
        // Add a backtrack task to do last
        if (task_stack.count > 0) {
            void *backtrack_task = make_task(new_var_id, -1, true, true);
            task_stack.add_to_front(backtrack_task);
        }
        bool first_choice;
        if (State::assignment_method == 0) {
            // Pick greedy
            first_choice = new_var_sign;
        } else if (State::assignment_method == 1) {
            // Pick opposite of greedy
            first_choice = !new_var_sign;
        } else if (State::assignment_method == 2) {
            // Always set true
            first_choice = true;
        } else {
            // Always set fakse
            first_choice = false;
        }
        void *important_task = make_task(new_var_id, -1, first_choice);
        void *other_task = make_task(new_var_id, -1, !first_choice);
        task_stack.add_to_front(other_task);
        task_stack.add_to_front(important_task);
        State::num_non_trivial_tasks += 2;
        cnf.true_assignment_statuses[new_var_id] = 'q';
        cnf.false_assignment_statuses[new_var_id] = 'q';
        if (PRINT_LEVEL >= 3) printf("%sPID %d: variable %d explicitly decided\n", cnf.depth_str.c_str(), State::pid, new_var_id);
        return 2;
    }
}

// Displays data structure data for debugging purposes
void State::print_data(Cnf &cnf, Deque &task_stack, std::string prefix_str) {
    if (PRINT_LEVEL > 1) cnf.print_task_stack(prefix_str, task_stack);
    if (PRINT_LEVEL > 2) cnf.print_edit_stack(prefix_str, (cnf.eedit_stack));
    if (PRINT_LEVEL > 1) cnf.print_cnf(prefix_str, cnf.depth_str, true);
    if (PRINT_LEVEL > 5) print_compressed(
        cnf.pid, prefix_str, cnf.depth_str, cnf.to_int_rep(), cnf.work_ints);
    if (PRINT_LEVEL > 5) print_compressed(
        cnf.pid, prefix_str, cnf.depth_str, cnf.oldest_compressed, cnf.work_ints);
    if (PRINT_LEVEL > 5) {
        std::string drop_str = "regular dropped [";
        for (int i = 0; i < cnf.clauses.num_indexed; i++) {
            if (cnf.clauses.clause_is_dropped(i)) {
                drop_str.append(std::to_string(i));
                drop_str.append(", ");
            }
        }
        drop_str.append("]");
        printf("%sPID %d: %s %s\n", cnf.depth_str.c_str(), State::pid, prefix_str.c_str(), drop_str.c_str());
        drop_str = "conflict dropped [";
        for (int i = 0; i < cnf.clauses.num_conflict_indexed; i++) {
            int clause_id = cnf.clauses.max_indexable + i;
            if (cnf.clauses.clause_is_dropped(clause_id)) {
                drop_str.append(std::to_string(clause_id));
                drop_str.append(", ");
            }
        }
        drop_str.append("]");
        printf("%sPID %d: %s %s\n", cnf.depth_str.c_str(), State::pid, prefix_str.c_str(), drop_str.c_str());
    }
}

// Runs one iteration of the solver
bool State::solve_iteration(
        Cnf &cnf, 
        Deque &task_stack, 
        Interconnect &interconnect) 
    {
    State::calls_to_solve++;
    assert(State::num_non_trivial_tasks > 0);
    assert(task_stack_invariant(cnf, task_stack, State::num_non_trivial_tasks));
    if (PRINT_LEVEL >= 3) printf("\n");
    bool should_recurse = recurse_required(task_stack);
    Clause conflict_clause;
    int conflict_id;
    Task task = get_task(task_stack);
    int var_id = task.var_id;
    int decided_var_id = var_id;
    int assignment = task.assignment;
    int implier = task.implier;
    if (task.is_backtrack) { // Children backtracked, need to backtrack ourselves
        print_data(cnf, task_stack, "Children backtrack");
        cnf.backtrack();
        return false;
    } else {
        State::num_non_trivial_tasks--;
    }
    if (should_recurse) {
        cnf.recurse();
    }
    if ((State::current_cycle % CYCLES_TO_PRINT_PROGRESS == 0) && PRINT_PROGRESS) {
        print_progress(cnf, task_stack);
        if (State::current_cycle == std::max(
            CYCLES_TO_PRINT_PROGRESS, CYCLES_TO_RECEIVE_MESSAGES)) {
            State::current_cycle = 0;
        }
    }
    State::current_cycle++;
    while (true) {
        if (PRINT_LEVEL > 3) print_data(cnf, task_stack, "Loop start");
        bool propagate_result = cnf.propagate_assignment(
            var_id, assignment, implier, &conflict_id);
        if (State::use_smart_prop) {
            propagate_result = propagate_result && cnf.smart_propagate_assignment(
                var_id, assignment, implier, &conflict_id);
        }
        if (!propagate_result) {
            print_data(cnf, task_stack, "Prop fail");
            if (ENABLE_CONFLICT_RESOLUTION && task_stack.count > 0) {
                bool resolution_result = cnf.conflict_resolution_uid(
                    conflict_id, conflict_clause, decided_var_id);
                if (resolution_result) {
                    handle_local_conflict_clause(
                        cnf, task_stack, conflict_clause, interconnect);
                    print_data(cnf, task_stack, "Post-handle local conflict clause");

                    // a unit prop is forced
                    // int ct = 0;
                    // for (int i = 0; i < conflict_clause.num_literals; i++) {
                    //     if (!(cnf.assigned_true[conflict_clause.literal_variable_ids[i]] || cnf.assigned_false[conflict_clause.literal_variable_ids[i]])) {
                    //         ct++;
                    //     }
                    // }
                    // assert(ct == 1);
                    // don't return false - continue to unit prop code later on

                    // backtracking has perhaps invalidated decided_var_id
                    // TODO: UNSURE HOW TO TEST CORRECTNESS
                    if (task_stack.count == 0) {
                        if (cnf.get_local_edit_count() > 0) {
                            Deque local_edits = *((Deque *)(cnf.eedit_stack.peak_front()));
                            decided_var_id = (*(FormulaEdit *)(local_edits.peak_back())).edit_id;
                        } else {
                            decided_var_id = -1; // calc should begin with unit prop
                        }
                    } else {
                        decided_var_id = peak_task(task_stack).var_id;
                    }
                } else {
                    cnf.backtrack();
                    return false;
                }
            } else {
                cnf.backtrack();
                return false;
            }
        }
        assert(task_stack_invariant(
            cnf, task_stack, State::num_non_trivial_tasks));
        if (cnf.clauses.get_linked_list_size() == 0) {
            if (PRINT_PROGRESS) print_progress(cnf, task_stack);
            cnf.assign_remaining();
            print_data(cnf, task_stack, "Base case success");
            return true;
        }
        if (PRINT_LEVEL > 3) print_data(cnf, task_stack, "Loop end");
        int num_added = add_tasks_from_formula(cnf, task_stack);
        if (num_added == 1) {
            Task task = get_task(task_stack);
            var_id = task.var_id;
            assignment = task.assignment;
            implier = task.implier;
            State::num_non_trivial_tasks--;

            if (decided_var_id == -1) {
                decided_var_id = var_id;
            }
            continue;
        }
        return false;
    }
}

// Continues solve operation, returns true iff a solution was found by
// the current thread.
bool State::solve(Cnf &cnf, Deque &task_stack, Interconnect &interconnect) {
    if (interconnect.pid == 0) {
        add_tasks_from_formula(cnf, task_stack);
        assert(task_stack.count > 0);
    }
    while (!State::process_finished) {
        interconnect.clean_dead_messages();
        if (out_of_work()) {
            // printf("pid %d oow\n", pid);
            bool found_work = get_work_from_interconnect_stash(
                cnf, task_stack, interconnect);
            if (!found_work) {
                ask_for_work(cnf, task_stack, interconnect);
            }
        }
        Message message;
        while (out_of_work() && !State::process_finished) {
            bool message_received = interconnect.async_receive_message(message);
            if (message_received) {
                // printf("PID %d OOW\n", pid);
                handle_message(message, cnf, task_stack, interconnect);
                // if (!State::process_finished) printf("PID %d now has work\n", pid);
            }
        }
        if (State::process_finished) break;
        bool result = solve_iteration(cnf, task_stack, interconnect);
        if (result) {
            assert(State::num_non_trivial_tasks >= 0);
            printf("success from pid %d!\n", pid);
            abort_others(interconnect, true);
            abort_process(cnf, task_stack, interconnect, true);
            return true;
        }
        if (current_cycle % CYCLES_TO_RECEIVE_MESSAGES == 0) {
            while (interconnect.async_receive_message(message) && !State::process_finished) {
                handle_message(message, cnf, task_stack, interconnect);
                // NICE: serve work here?
            }
            if (State::process_finished) break;
            if (State::current_cycle == std::max(
                CYCLES_TO_PRINT_PROGRESS, CYCLES_TO_RECEIVE_MESSAGES)) {
                State::current_cycle = 0;
            }
        }
        if (!out_of_work()) { // Serve ourselves before others
            while (workers_requesting() && can_give_work(task_stack, interconnect)) {
                give_work(cnf, task_stack, interconnect);
            }
        }
    }
    return false;
}