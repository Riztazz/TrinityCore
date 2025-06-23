DROP TABLE IF EXISTS `map_partitions`;

CREATE TABLE IF NOT EXISTS `map_partitions`  (
  `id` INT NOT NULL AUTO_INCREMENT,
  `mapId` INT NOT NULL,
  `partitionId` INT NOT NULL,
  `priority` INT NOT NULL,
  `polygon` TEXT NOT NULL,
  PRIMARY KEY (`id`)
);