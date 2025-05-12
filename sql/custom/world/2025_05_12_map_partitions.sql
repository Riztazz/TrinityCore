CREATE TABLE `map_partitions`  (
  `id` INT NOT NULL AUTO_INCREMENT,
  `mapId` INT NOT NULL,
  `partitionId` INT NOT NULL,
  `polygon` TEXT NOT NULL,
  PRIMARY KEY (`id`)
);