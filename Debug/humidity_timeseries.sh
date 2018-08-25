while true; do
  timestamp=$(date +%s)
  humidity=$(curl --request GET --url http://192.168.178.84/soil)
  hum=$(curl --request GET --url http://192.168.178.84/hum)
  temp=$(curl --request GET --url http://192.168.178.84/temp)
  echo "$timestamp\t$humidity\t$hum\t$temp"  >> ./soil_data2.txt
  sleep 300
done

#$(date '+%Y-%m-%d %H:%M:%S')
