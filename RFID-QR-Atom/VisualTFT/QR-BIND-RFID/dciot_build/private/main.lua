
function on_init()
	uart_free_protocol = 1
	make_qr()	
end

function  on_sd_inserted(dir)
    sd_dir = dir
	local write_byte_Tb = {0x41, 0x42, 0x43}
    set_text(0,2,sd_dir..'/'..'1.txt')	
	--my_write_filedata(sd_dir..'/'..'1.txt', str, add_write)	
    local str = 'abcdef'
	local open_state = file_open(sd_dir..'/aaa/'..'1.txt', 0x02|0x08)	
	if (open_state == true)
	then
		set_text(0,3,"open ok")	
	else
		set_text(0,3,"open fail")		
	end
	write_byte_Tb[0]=0x44
	file_write(write_byte_Tb)
	file_close()
	
end

--系?每隔1秒?自??用此回?函?。
--function on_systick()
--end

--定?器超?回?函?
function on_timer(timer_id)
	print("Timeout")	
    set_backlight(10)	
end

function on_press(state,x,y)
  print("screen touched")
  set_backlight(100)
  print("Start timer")	
  start_timer(0, 30000, 0, 1)
end

--??面需要切??，?行此回?函?，screen?目??面。
--function on_screen_change(screen)
--end

--用?触摸修改控件后，?行此回?函?。
--{"rfid":"rfid_num","id1":"group_id","id2":"site_id"}
function on_control_notify(screen,control,value)
	--print("Hi")
    --set_backlight(100)	
    if screen == 0 
    then
		make_qr()	
 		--uart_doorState(0x23)
    end
end

function make_qr()
    set_backlight(100)
 	print("Start timer")	
	start_timer(0, 30000, 0, 1)

	local string rfid_num   =get_text(0, 4) 
	local string site_id      =get_text(0, 3) 
	local string group_id   =get_text(0, 2) 
 	-- .. 為 string Concatenate	
	local string qr_string= "{" .. "\"rfid\":" .. "\"" .. rfid_num .. "\"" .. "," 
		                                 .. "\"id1\":"  .. "\"" .. group_id .. "\"" .. ","
									     .. "\"id2\":"  .. "\"" .. site_id    .. "\"" ..  "}" 
	set_text(0,1,qr_string)	
	beep(100)
end

function uart_doorState(state)
    local door_buff = {}    
    door_buff[0] = 0x5A
    door_buff[1] = 0x5A
    door_buff[2] = 0x07
    door_buff[3] = 0x82
    door_buff[4] = 0x00
    door_buff[5] = 0x01
    door_buff[6] = state

    uart_send_data(door_buff)
end

function on_uart_recv_data(packet)
	-- 從 external MCU 傳送 EE B5【自定數据】FF FC FF FF
	-- 例如 EE B5 00 11 FF FC FF FF
	--print(packet[0]) -- 0xEE
	--print(packet[1]) -- 0xB5
	--print(packet[2]) -- 0x00
	--print(packet[3]) -- 0x11
	make_qr()
end