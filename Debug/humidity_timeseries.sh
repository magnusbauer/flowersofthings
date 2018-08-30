while true; do
  timestamp=$(date +%s)
  humidity=$(curl --request GET --url http://YOUR-IP-or-DNS/soil)
  hum=$(curl --request GET --url http://YOUR-IP-or-DNS/hum)
  temp=$(curl --request GET --url http://YOUR-IP-or-DNS/temp)
  echo "$timestamp\t$humidity\t$hum\t$temp"  >> ./soil_data.txt
  sleep 300
done
