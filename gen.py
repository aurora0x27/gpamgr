import random

months = ["Jan","Feb","Mar","Apr","May","Jun",
          "Jul","Aug","Sep","Oct","Nov","Dec"]

first_name_char = [
    'wei', 'min', 'jie', 'hao', 'qiang', 'jun', 'yan', 'fang', 'juan', 'ting',
    'lei', 'chao', 'yang', 'ping', 'gang', 'hua', 'tao', 'yi', 'chen', 'yue',
    'feng', 'yu', 'bin', 'jing', 'lin', 'xin', 'bo', 'ming', 'jian', 'hong',
    'xu', 'shu', 'ying', 'rong', 'yuan', 'xiang', 'xi', 'zen', 'ran', 'han',
    'zi', 'mu', 'xiao', 'fan', 'yao', 'mo', 'kun', 'peng', 'zhao', 'dan'
]

family_name = [
    'li', 'wang', 'zhang', 'liu', 'chen', 'yang', 'zhao', 'huang', 'zhou', 'wu',
    'xu', 'sun', 'hu', 'zhu', 'gao', 'lin', 'he', 'guo', 'ma', 'luo',
    'liang', 'song', 'zheng', 'xie', 'han', 'tang', 'feng', 'yu', 'dong', 'xiao',
    'cheng', 'cao', 'yuan', 'deng', 'xu', 'fu', 'shen', 'zeng', 'peng', 'lv',
    'su', 'lu', 'jiang', 'cai', 'jia', 'ding', 'wei', 'xue', 'pan', 'du',
    'dai', 'xia', 'zhong', 'wang', 'tian', 'ren', 'jiang', 'fan', 'fang', 'shi',
    'yao', 'tan', 'liao', 'zou', 'xiong', 'jin', 'lu', 'hao', 'kong', 'bai',
    'cui', 'kang', 'mao', 'qiu', 'qin', 'jiang', 'shi', 'gu', 'hou', 'shao',
    'meng', 'long', 'wan', 'duan', 'lei', 'qian', 'tang', 'yin', 'li', 'yi',
    'chang', 'wu', 'qiao', 'he', 'lai', 'gong', 'wen'
]

def generate_random_name():
    surname = random.choice(family_name)
    name_len = random.randint(1, 2)
    given_name = "".join(random.choices(first_name_char, k=name_len))
    return f"{surname.capitalize()} {given_name.capitalize()}"

# def rand_name():
#     m = random.choice(months)
#     s = ''.join(random.choices(string.ascii_lowercase + string.digits,
#                                k=random.randint(1,6)))
#     return f'{m} {s}'

print('.create exam sid:int primary key, name:string, maths:float, physics:float, chemistry:float, biology:float')

for i in range(1, 1000001):
    print(f'insert into exam values ({i}, "{generate_random_name()}", '
          f'{random.uniform(0,100):.2f}, '
          f'{random.uniform(0,100):.2f}, '
          f'{random.uniform(0,100):.2f}, '
          f'{random.uniform(0,100):.2f});')
