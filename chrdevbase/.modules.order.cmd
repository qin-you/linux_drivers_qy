cmd_/home/ubuntu/linux_drivers_qy/chrdevbase/modules.order := {   echo /home/ubuntu/linux_drivers_qy/chrdevbase/chrdevbase.ko; :; } | awk '!x[$$0]++' - > /home/ubuntu/linux_drivers_qy/chrdevbase/modules.order